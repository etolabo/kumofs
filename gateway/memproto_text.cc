#include "gateway/memproto_text.h"
#include "memproto/memproto_text.h"
#include <stdexcept>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <algorithm>
#include <memory>

namespace kumo {


static const size_t MEMPROTO_TEXT_INITIAL_ALLOCATION_SIZE = 2048;
static const size_t MEMPROTO_TEXT_RESERVE_SIZE = 1024;


MemprotoText::MemprotoText(int lsock) :
	m_lsock(lsock) { }

MemprotoText::~MemprotoText() {}


void MemprotoText::accepted(void* data, int fd)
{
	Gateway* gw = reinterpret_cast<Gateway*>(data);
	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(-fd));
		gw->signal_end(SIGTERM);
		return;
	}
	mp::set_nonblock(fd);
	mp::iothreads::add<Connection>(fd, gw);
}

void MemprotoText::listen(Gateway* gw)
{
	mp::iothreads::listen(m_lsock,
			&MemprotoText::accepted,
			reinterpret_cast<void*>(gw));
}


class MemprotoText::Connection : public iothreads::handler {
public:
	Connection(int fd, Gateway* gw);
	~Connection();

public:
	void read_event();

private:
	inline int memproto_get(
			const char** keys, unsigned* key_lens, unsigned key_num);

	inline int memproto_set(
			const char* key, unsigned key_len,
			unsigned short flags, uint64_t exptime,
			const char* data, unsigned data_len,
			bool noreply);

	inline int memproto_delete(
			const char* key, unsigned key_len,
			uint64_t time, bool noreply);

private:
	memproto_text m_memproto;
	char* m_buffer;
	size_t m_free;
	size_t m_used;
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


	struct LifeKeeper {
		LifeKeeper(shared_zone& z) : m(z) { }
		~LifeKeeper() { }
	private:
		shared_zone m;
		LifeKeeper();
	};


	struct Responder {
		Responder(int fd, SharedValid& valid) :
			m_fd(fd), m_valid(valid) { }
		~Responder() { }

		bool is_valid() const { return *m_valid; }

		int fd() const { return m_fd; }

		void send_data(const char* buf, size_t buflen);
		void send_datav(struct iovec* vb, size_t count, shared_zone& life);

	private:
		int m_fd;
		SharedValid m_valid;
	};

	struct ResGet : Responder {
		ResGet(int fd, SharedValid& valid) :
			Responder(fd, valid) { }
		~ResGet() { }
		void response(get_response& res);
	};

	struct ResMultiGet : Responder {
		ResMultiGet(int fd, SharedValid& valid) :
			Responder(fd, valid) { }
		~ResMultiGet() { }
		void set_count(size_t c) { m_count = c; }
		void response(get_response& res);
	private:
		size_t m_count;
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


	template <typename T>
	char* alloc_responder_buf(shared_zone& z, size_t size, T** rp);

	template <typename T>
	static void object_destruct_free(void* data);

private:
	Connection();
	Connection(const Connection&);
};


MemprotoText::Connection::Connection(int fd, Gateway* gw) :
	mp::iothreads::handler(fd),
	m_buffer(NULL),
	m_free(0),
	m_used(0),
	m_off(0),
	m_gw(gw),
	m_valid(new bool(true))
{
	int (*cmd_get)(void*, const char**, unsigned*, unsigned) =
		&mp::object_callback<int (const char**, unsigned*, unsigned)>::
				mem_fun<Connection, &Connection::memproto_get>;

	int (*cmd_set)(void*, const char*, unsigned,
			unsigned short, uint64_t,
			const char*, unsigned, bool) =
		&mp::object_callback<int (const char*, unsigned,
				unsigned short, uint64_t,
				const char*, unsigned,
				bool)>::
				mem_fun<Connection, &Connection::memproto_set>;

	int (*cmd_delete)(void*, const char*, unsigned,
			uint64_t, bool) =
		&mp::object_callback<int (const char*, unsigned,
				uint64_t, bool)>::
				mem_fun<Connection, &Connection::memproto_delete>;

	memproto_text_callback cb = {
		cmd_get,     // get
		cmd_set,     // set
		NULL,        // replace
		NULL,        // append
		NULL,        // prepend
		NULL,        // cas
		cmd_delete,  // delete
	};

	memproto_text_init(&m_memproto, &cb, this);
}

MemprotoText::Connection::~Connection()
{
	*m_valid = false;
	::free(m_buffer);
}


void MemprotoText::Connection::read_event()
try {
	if(m_free < MEMPROTO_TEXT_RESERVE_SIZE) {
		size_t nsize;
		if(m_buffer == NULL) { nsize = MEMPROTO_TEXT_INITIAL_ALLOCATION_SIZE; }
		else { nsize = (m_free + m_used) * 2; }
		char* tmp = (char*)::realloc(m_buffer, nsize);
		if(!tmp) { throw std::bad_alloc(); }
		m_buffer = tmp;
		m_free = nsize - m_used;
	}

	ssize_t rl = ::read(fd(), m_buffer+m_used, m_free);
	if(rl < 0) {
		if(errno == EAGAIN || errno == EINTR) {
			return;
		} else {
			throw std::runtime_error("read error");
		}
	} else if(rl == 0) {
		throw std::runtime_error("connection closed");
	}

	m_used += rl;
	m_free -= rl;

	int ret = memproto_text_execute(&m_memproto, m_buffer, m_used, &m_off);

	if(ret < 0) {  // parse failed
		throw std::runtime_error("parse error");

	} else if(ret == 0) {  // continue
		return;

	} else {  // parse finished
		if(m_off == m_used) {
			m_free += m_used;
			m_used = 0;
		} else {
			::memmove(m_buffer, m_buffer+m_off, m_used-m_off);
		}
		m_off = 0;
	}

} catch (std::runtime_error& e) {
	LOG_DEBUG("memcached text protocol error: ",e.what());
	throw;
} catch (...) {
	LOG_DEBUG("memcached text protocol error: unknown error");
	throw;
}


template <typename T>
void MemprotoText::Connection::object_destruct_free(void* data)
{
	reinterpret_cast<T*>(data)->~T();
	free(data);
}

template <typename T>
char* MemprotoText::Connection::alloc_responder_buf(shared_zone& z, size_t size, T** rp)
{
	void* b = malloc(sizeof(T) + size);
	if(!b) { throw std::bad_alloc(); }
	try {
		*rp = new (b) T(fd(), m_valid);
	} catch (...) { free(b); throw; }
	try {
		z->push_finalizer(&object_destruct_free<T>, b);
	} catch (...) { (*rp)->~T(); free(b); throw; }
	return ((char*)b) + sizeof(T);
}


void MemprotoText::Connection::Responder::send_data(
		const char* buf, size_t buflen)
{
	mp::iothreads::send_data(m_fd, buf, buflen);
}

void MemprotoText::Connection::Responder::send_datav(
		struct iovec* vb, size_t count, shared_zone& life)
{
	mp::iothreads::writer::reqvec vr[count];
	for(size_t i=0; i < count-1; ++i) {
		vr[i] = mp::iothreads::writer::reqvec();
	}
	vr[count-1] = mp::iothreads::writer::reqvec(
			&mp::iothreads::writer::finalize_delete<LifeKeeper>,
			new LifeKeeper(life), true);
	mp::iothreads::send_datav(m_fd, vb, vr, count);
}


namespace {
static const char* const NOT_SUPPORTED_REPLY = "CLIENT_ERROR supported\r\n";
static const char* const GET_FAILED_REPLY    = "SERVER_ERROR get failed\r\n";
static const char* const STORE_FAILED_REPLY  = "SERVER_ERROR store failed\r\n";
static const char* const DELETE_FAILED_REPLY = "SERVER_ERROR delete failed\r\n";
}  // noname namespace


int MemprotoText::Connection::memproto_get(
		const char** keys, unsigned* key_lens, unsigned key_num)
{
	LOG_TRACE("get");

	if(key_num == 1) {
		const char* key = keys[0];
		unsigned keylen = key_lens[0];

		ResGet* ctx;
		get_request req;
		req.keylen = keylen;
		req.key = alloc_responder_buf(req.life, keylen, &ctx);
		memcpy((void*)req.key, key, keylen);
		req.callback = &mp::object_callback<void (get_response&)>
			::mem_fun<ResGet, &ResGet::response>;
		req.user = (void*)ctx;

		m_gw->submit(req);

	} else {
		size_t keylen_sum = 0;
		for(unsigned i=0; i < key_num; ++i) {
			keylen_sum += key_lens[i];
		}

		ResMultiGet* ctx;
		get_request req;
		char* zp = alloc_responder_buf(req.life, keylen_sum, &ctx);
		req.callback = &mp::object_callback<void (get_response&)>
			::mem_fun<ResMultiGet, &ResMultiGet::response>;
		req.user = (void*)ctx;

		ctx->set_count(key_num);
		for(unsigned i=0; i < key_num; ++i) {
			memcpy(zp, keys[i], key_lens[i]);
			req.key = zp;
			req.keylen = key_lens[i];
			m_gw->submit(req);
			zp += key_lens[i];
		}
	}

	return 0;
}


int MemprotoText::Connection::memproto_set(
		const char* key, unsigned key_len,
		unsigned short flags, uint64_t exptime,
		const char* data, unsigned data_len,
		bool noreply)
{
	LOG_TRACE("set");

	if(flags || exptime) {
		send_data(NOT_SUPPORTED_REPLY, strlen(NOT_SUPPORTED_REPLY));
		return 0;
	}

	ResSet* ctx;
	set_request req;
	char* zp = alloc_responder_buf(req.life, key_len + data_len, &ctx);
	req.keylen = key_len;
	req.key = zp;
	memcpy((void*)req.key, key, key_len);
	req.vallen = data_len;
	req.val = zp + key_len;
	memcpy((void*)req.val, data, data_len);

	if(noreply) {
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
		const char* key, unsigned key_len,
		uint64_t time, bool noreply)
{
	LOG_TRACE("delete");

	if(time) {
		send_data(NOT_SUPPORTED_REPLY, strlen(NOT_SUPPORTED_REPLY));
		return 0;
	}

	ResDelete* ctx;
	delete_request req;
	req.keylen = key_len;
	req.key = alloc_responder_buf(req.life, key_len, &ctx);
	memcpy((void*)req.key, key, key_len);

	if(noreply) {
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

	struct iovec vb[6];
	vb[0].iov_base = const_cast<char*>("VALUE ");
	vb[0].iov_len  = 6;
	vb[1].iov_base = const_cast<char*>(res.key);
	vb[1].iov_len  = res.keylen;
	vb[2].iov_base = const_cast<char*>(" 0 ");
	vb[2].iov_len  = 3;
	char* numbuf = (char*)res.life->malloc(10+3);  // uint32 + \r\n\0
	vb[3].iov_base = numbuf;
	vb[3].iov_len  = sprintf(numbuf, "%u\r\n", res.vallen);
	vb[4].iov_base = const_cast<char*>(res.val);
	vb[4].iov_len  = res.vallen;
	vb[5].iov_base = const_cast<char*>("\r\nEND\r\n");
	vb[5].iov_len  = 7;
	send_datav(vb, 6, res.life);
}


void MemprotoText::Connection::ResMultiGet::response(get_response& res)
{
	if(!is_valid()) { return; }
	LOG_TRACE("get multi response ",m_count);

	--m_count;

	if(res.error || !res.val) {
		return;
	}

	struct iovec vb[6];
	vb[0].iov_base = const_cast<char*>("VALUE ");
	vb[0].iov_len  = 6;
	vb[1].iov_base = const_cast<char*>(res.key);
	vb[1].iov_len  = res.keylen;
	vb[2].iov_base = const_cast<char*>(" 0 ");
	vb[2].iov_len  = 3;
	char* numbuf = (char*)res.life->malloc(10+3);  // uint32 + \r\n\0
	vb[3].iov_base = numbuf;
	vb[3].iov_len  = sprintf(numbuf, "%u\r\n", res.vallen);
	vb[4].iov_base = const_cast<char*>(res.val);
	vb[4].iov_len  = res.vallen;
	if(m_count > 0) {
		vb[5].iov_base = const_cast<char*>("\r\n");
		vb[5].iov_len  = 2;
	} else {
		vb[5].iov_base = const_cast<char*>("\r\nEND\r\n");
		vb[5].iov_len  = 7;
	}
	send_datav(vb, 6, res.life);
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

