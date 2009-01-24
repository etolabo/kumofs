#include "gateway/memtext.h"
#include "memproto/memtext.h"
#include <mp/pthread.h>
#include <mp/stream_buffer.h>
#include <mp/object_callback.h>
#include <stdexcept>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <algorithm>
#include <memory>

namespace kumo {


static const size_t MEMTEXT_INITIAL_ALLOCATION_SIZE = 16*1024;
static const size_t MEMTEXT_RESERVE_SIZE = 1024;


MemprotoText::MemprotoText(int lsock) :
	m_lsock(lsock) { }

MemprotoText::~MemprotoText() {}


void MemprotoText::accepted(Gateway* gw, int fd, int err)
{
	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(err));
		gw->signal_end();
		return;
	}
	LOG_DEBUG("accept memproto text user fd=",fd);
	wavy::add<Connection>(fd, gw);
}

void MemprotoText::listen(Gateway* gw)
{
	using namespace mp::placeholders;
	wavy::listen(m_lsock,
			mp::bind(&MemprotoText::accepted, gw, _1, _2));
}


class MemprotoText::Connection : public wavy::handler {
public:
	Connection(int fd, Gateway* gw);
	~Connection();

public:
	void read_event();

private:
	inline int memproto_get(
			memtext_command cmd,
			memtext_request_retrieval* r);

	inline int memproto_set(
			memtext_command cmd,
			memtext_request_storage* r);

	inline int memproto_delete(
			memtext_command cmd,
			memtext_request_delete* r);

private:
	memtext_parser m_memproto;
	mp::stream_buffer m_buffer;
	size_t m_off;
	Gateway* m_gw;

	typedef mp::shared_ptr<bool> SharedValid;
	SharedValid m_valid;

	typedef Gateway::get_request get_request;
	typedef Gateway::set_request set_request;
	typedef Gateway::delete_request delete_request;

	typedef Gateway::get_response get_response;
	typedef Gateway::set_response set_response;
	typedef Gateway::delete_response delete_response;

	typedef rpc::shared_zone shared_zone;


	struct Responder {
		Responder(int fd, SharedValid& valid) :
			m_fd(fd), m_valid(valid) { }
		~Responder() { }

		bool is_valid() const { return *m_valid; }

		int fd() const { return m_fd; }

	protected:
		inline void send_data(const char* buf, size_t buflen);
		inline void send_datav(struct iovec* vb, size_t count, shared_zone& life);

	private:
		int m_fd;
		SharedValid m_valid;
	};

	struct ResGet : Responder {
		ResGet(int fd, SharedValid& valid) :
			Responder(fd, valid) { }
		~ResGet() { }
		void response(get_response& res);
	private:
		char m_numbuf[3+10+3];  // " 0 " + uint32 + "\r\n\0"
		ResGet();
		ResGet(const ResGet&);
	};

	struct ResMultiGet : Responder {
		ResMultiGet(int fd, SharedValid& valid,
				struct iovec* vec, unsigned* count,
				struct iovec* qhead, unsigned qlen) :
			Responder(fd, valid),
			m_vec(vec), m_count(count),
			m_qhead(qhead), m_qlen(qlen) { }
		~ResMultiGet() { }
		void response(get_response& res);
	private:
		struct iovec* m_vec;
		unsigned *m_count;
		struct iovec* m_qhead;
		size_t m_qlen;
		char m_numbuf[3+10+3];  // " 0 " + uint32 + "\r\n\0"
	private:
		ResMultiGet();
		ResMultiGet(const ResMultiGet&);
	};

	struct ResSet : Responder {
		ResSet(int fd, SharedValid& valid) :
			Responder(fd, valid) { }
		~ResSet() { }
		void response(set_response& res);
		void no_response(set_response& res);
	};

	struct ResDelete : Responder {
		ResDelete(int fd, SharedValid& valid) :
			Responder(fd, valid) { }
		~ResDelete() { }
		void response(delete_response& res);
		void no_response(delete_response& res);
	};

private:
	Connection();
	Connection(const Connection&);
};


MemprotoText::Connection::Connection(int fd, Gateway* gw) :
	mp::wavy::handler(fd),
	m_buffer(MEMTEXT_INITIAL_ALLOCATION_SIZE),
	m_off(0),
	m_gw(gw),
	m_valid(new bool(true))
{
	int (*cmd_get)(void*, memtext_command, memtext_request_retrieval*) =
		&mp::object_callback<int (memtext_command, memtext_request_retrieval*)>::
				mem_fun<Connection, &Connection::memproto_get>;

	int (*cmd_set)(void*, memtext_command, memtext_request_storage*) =
		&mp::object_callback<int (memtext_command, memtext_request_storage*)>::
				mem_fun<Connection, &Connection::memproto_set>;

	int (*cmd_delete)(void*, memtext_command, memtext_request_delete*) =
		&mp::object_callback<int (memtext_command, memtext_request_delete*)>::
				mem_fun<Connection, &Connection::memproto_delete>;

	memtext_callback cb = {
		cmd_get,      // get
		cmd_set,      // set
		NULL,         // add
		NULL,         // replace
		NULL,         // append
		NULL,         // prepend
		NULL,         // cas
		cmd_delete,   // delete
		NULL,         // incr
		NULL,         // decr
	};

	memtext_init(&m_memproto, &cb, this);
}

MemprotoText::Connection::~Connection()
{
	*m_valid = false;
}


void MemprotoText::Connection::read_event()
try {
	m_buffer.reserve_buffer(MEMTEXT_RESERVE_SIZE);

	ssize_t rl = ::read(fd(), m_buffer.buffer(), m_buffer.buffer_capacity());
	if(rl < 0) {
		if(errno == EAGAIN || errno == EINTR) {
			return;
		} else {
			throw std::runtime_error("read error");
		}
	} else if(rl == 0) {
		throw std::runtime_error("connection closed");
	}

	m_buffer.buffer_consumed(rl);

	do {
		int ret = memtext_execute(&m_memproto,
				(char*)m_buffer.data(), m_buffer.data_size(), &m_off);
		if(ret < 0) {
			throw std::runtime_error("parse error");
		}
		if(ret == 0) { return; }
		m_buffer.data_used(m_off);
		m_off = 0;
	} while(m_buffer.data_size() > 0);

} catch (std::exception& e) {
	LOG_DEBUG("memcached text protocol error: ",e.what());
	throw;
} catch (...) {
	LOG_DEBUG("memcached text protocol error: unknown error");
	throw;
}


void MemprotoText::Connection::Responder::send_data(
		const char* buf, size_t buflen)
{
	wavy::write(m_fd, buf, buflen);
}

void MemprotoText::Connection::Responder::send_datav(
		struct iovec* vb, size_t count, shared_zone& life)
{
	wavy::request req(&mp::object_delete<shared_zone>, new shared_zone(life));
	wavy::writev(m_fd, vb, count, req);
}


namespace {
static const char* const NOT_SUPPORTED_REPLY = "CLIENT_ERROR supported\r\n";
static const char* const GET_FAILED_REPLY    = "SERVER_ERROR get failed\r\n";
static const char* const STORE_FAILED_REPLY  = "SERVER_ERROR store failed\r\n";
static const char* const DELETE_FAILED_REPLY = "SERVER_ERROR delete failed\r\n";
}  // noname namespace

#define RELEASE_REFERENCE(life) \
	shared_zone life(new msgpack::zone()); \
	life->push_finalizer(&mp::object_delete<mp::stream_buffer::reference>, \
				m_buffer.release());

int MemprotoText::Connection::memproto_get(
		memtext_command cmd,
		memtext_request_retrieval* r)
{
	LOG_TRACE("get");
	RELEASE_REFERENCE(life);

	if(r->key_num == 1) {
		const char* key = r->key[0];
		unsigned keylen = r->key_len[0];

		ResGet* ctx = life->allocate<ResGet>(fd(), m_valid);
		get_request req;
		req.key = key;
		req.keylen = keylen;
		req.hash = Gateway::stdhash(req.key, req.keylen);
		req.callback = &mp::object_callback<void (get_response&)>
			::mem_fun<ResGet, &ResGet::response>;
		req.user = (void*)ctx;
		req.life = life;

		m_gw->submit(req);

	} else {
		ResMultiGet* ctxs[r->key_num];
		unsigned* count = life->allocate<unsigned>(r->key_num);

		size_t qlen = r->key_num * 5 + 1;  // +1: "END\r\n"
		struct iovec* qhead = (struct iovec*)life->malloc(sizeof(struct iovec) * qlen);
		qhead[qlen-1].iov_base = const_cast<char*>("END\r\n");
		qhead[qlen-1].iov_len = 5;

		for(unsigned i=0; i < r->key_num; ++i) {
			ctxs[i] = life->allocate<ResMultiGet>(fd(), m_valid,
					qhead + i*5, count, qhead, qlen);
		}

		get_request req;
		req.callback = &mp::object_callback<void (get_response&)>
			::mem_fun<ResMultiGet, &ResMultiGet::response>;
		req.life = life;

		for(unsigned i=0; i < r->key_num; ++i) {
			// don't use shared zone. msgpack::allocate is not thread-safe.
			req.user = (void*)ctxs[i];
			req.key = r->key[i];
			req.keylen = r->key_len[i];
			req.hash = Gateway::stdhash(req.key, req.keylen);
			m_gw->submit(req);
		}
	}

	return 0;
}


int MemprotoText::Connection::memproto_set(
		memtext_command cmd,
		memtext_request_storage* r)
{
	LOG_TRACE("set");
	RELEASE_REFERENCE(life);

	if(r->flags || r->exptime) {
		wavy::write(fd(), NOT_SUPPORTED_REPLY, strlen(NOT_SUPPORTED_REPLY));
		return 0;
	}

	ResSet* ctx = life->allocate<ResSet>(fd(), m_valid);
	set_request req;
	req.key = r->key;
	req.keylen = r->key_len;
	req.hash = Gateway::stdhash(req.key, req.keylen);
	req.val = r->data;
	req.vallen = r->data_len;
	req.life = life;

	if(r->noreply) {
		req.callback = &mp::object_callback<void (set_response&)>
			::mem_fun<ResSet, &ResSet::no_response>;
	} else {
		req.callback = &mp::object_callback<void (set_response&)>
			::mem_fun<ResSet, &ResSet::response>;
	}
	req.user = ctx;

	m_gw->submit(req);

	return 0;
}


int MemprotoText::Connection::memproto_delete(
		memtext_command cmd,
		memtext_request_delete* r)
{
	LOG_TRACE("delete");
	RELEASE_REFERENCE(life);

	if(r->exptime) {
		wavy::write(fd(), NOT_SUPPORTED_REPLY, strlen(NOT_SUPPORTED_REPLY));
		return 0;
	}

	ResDelete* ctx = life->allocate<ResDelete>(fd(), m_valid);
	delete_request req;
	req.key = r->key;
	req.keylen = r->key_len;
	req.hash = Gateway::stdhash(req.key, req.keylen);
	req.life = life;

	if(r->noreply) {
		req.callback = &mp::object_callback<void (delete_response&)>
			::mem_fun<ResDelete, &ResDelete::no_response>;
	} else {
		req.callback = &mp::object_callback<void (delete_response&)>
			::mem_fun<ResDelete, &ResDelete::response>;
	}
	req.user = ctx;

	m_gw->submit(req);

	return 0;
}



void MemprotoText::Connection::ResGet::response(get_response& res)
{
	if(!is_valid()) { return; }
	LOG_TRACE("get response");

	if(res.error) {
		send_data(GET_FAILED_REPLY, strlen(GET_FAILED_REPLY));
		return;
	}

	if(!res.val) {
		send_data("END\r\n", 5);
		return;
	}

	struct iovec vb[5];
	vb[0].iov_base = const_cast<char*>("VALUE ");
	vb[0].iov_len  = 6;
	vb[1].iov_base = const_cast<char*>(res.key);
	vb[1].iov_len  = res.keylen;
	vb[2].iov_base = m_numbuf;
	vb[2].iov_len  = sprintf(m_numbuf, " 0 %u\r\n", res.vallen);
	vb[3].iov_base = const_cast<char*>(res.val);
	vb[3].iov_len  = res.vallen;
	vb[4].iov_base = const_cast<char*>("\r\nEND\r\n");
	vb[4].iov_len  = 7;
	send_datav(vb, 5, res.life);
}


void MemprotoText::Connection::ResMultiGet::response(get_response& res)
{
	if(!is_valid()) { return; }
	LOG_TRACE("get multi response ",m_count);

	if(res.error || !res.val) {
		memset(m_vec, 0, sizeof(struct iovec)*5);
		goto filled;
	}

	// don't use shared zone. msgpack::allocate is not thread-safe.
	m_vec[0].iov_base = const_cast<char*>("VALUE ");
	m_vec[0].iov_len  = 6;
	m_vec[1].iov_base = const_cast<char*>(res.key);
	m_vec[1].iov_len  = res.keylen;
	m_vec[2].iov_base = m_numbuf;
	m_vec[2].iov_len  = sprintf(m_numbuf, " 0 %u\r\n", res.vallen);
	m_vec[3].iov_base = const_cast<char*>(res.val);
	m_vec[3].iov_len  = res.vallen;
	m_vec[4].iov_base = const_cast<char*>("\r\n");
	m_vec[4].iov_len  = 2;

filled:
	if(__sync_sub_and_fetch(m_count, 1) == 0) {
		send_datav(m_qhead, m_qlen, res.life);
	}
}


void MemprotoText::Connection::ResSet::response(set_response& res)
{
	if(!is_valid()) { return; }
	LOG_TRACE("set response");

	if(res.error) {
		send_data(STORE_FAILED_REPLY, strlen(STORE_FAILED_REPLY));
		return;
	}

	send_data("STORED\r\n", 8);
}

void MemprotoText::Connection::ResSet::no_response(set_response& res)
{ }


void MemprotoText::Connection::ResDelete::response(delete_response& res)
{
	if(!is_valid()) { return; }
	LOG_TRACE("delete response");

	if(res.error) {
		send_data(DELETE_FAILED_REPLY, strlen(DELETE_FAILED_REPLY));
		return;
	}
	if(res.deleted) {
		send_data("DELETED\r\n", 9);
	} else {
		send_data("NOT FOUND\r\n", 11);
	}
}

void MemprotoText::Connection::ResDelete::no_response(delete_response& res)
{ }


}  // namespace kumo

