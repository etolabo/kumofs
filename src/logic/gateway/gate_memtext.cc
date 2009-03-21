#define __STDC_LIMIT_MACROS
#define __STDC_FORMAT_MACROS
#include "gateway/gate_memtext.h"
#include "gateway/memproto/memtext.h"
#include "log/mlogger.h"
#include "gateway/framework.h"  // FIXME net->signal_end()
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
#include <inttypes.h>

namespace kumo {


static const size_t MEMTEXT_INITIAL_ALLOCATION_SIZE = 32*1024;
static const size_t MEMTEXT_RESERVE_SIZE = 4*1024;


Memtext::Memtext(int lsock) :
	m_lsock(lsock) { }

Memtext::~Memtext() {}


void Memtext::accepted(int fd, int err)
{
	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(err));
		gateway::net->signal_end();  // FIXME gateway::fatal_end()
		return;
	}
#ifndef NO_TCP_NODELAY
	int on = 1;
	::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));  // ignore error
#endif
#ifndef NO_SO_LINGER
	struct linger opt = {0, 0};
	::setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&opt, sizeof(opt));  // ignore error
#endif
	LOG_DEBUG("accept memproto text user fd=",fd);
	wavy::add<Connection>(fd);
}

void Memtext::listen()
{
	using namespace mp::placeholders;
	wavy::listen(m_lsock,
			mp::bind(&Memtext::accepted, _1, _2));
}


class Memtext::Connection : public wavy::handler {
public:
	Connection(int fd);
	~Connection();

public:
	void read_event();

private:
	int request_get(
			memtext_command cmd,
			memtext_request_retrieval* r);

	int request_gets(
			memtext_command cmd,
			memtext_request_retrieval* r);

	int request_set(
			memtext_command cmd,
			memtext_request_storage* r);

	int request_delete(
			memtext_command cmd,
			memtext_request_delete* r);

private:
	int request_get_real(
			memtext_command cmd,
			memtext_request_retrieval* r,
			bool require_cas);

private:
	memtext_parser m_memproto;
	mp::stream_buffer m_buffer;
	size_t m_off;

	typedef gateway::get_request gw_get_request;
	typedef gateway::set_request gw_set_request;
	typedef gateway::delete_request gw_delete_request;

	typedef gateway::get_response gw_get_response;
	typedef gateway::set_response gw_set_response;
	typedef gateway::delete_response gw_delete_response;

	typedef rpc::shared_zone shared_zone;


	class context {
	public:
		context(int fd);
		~context();

		int is_valid() const;
		void invalidate();

		void send_data(const char* buf, size_t buflen);
		void send_datav(struct iovec* vb, size_t count, shared_zone& life);
		void send_datav(struct iovec* vb, wavy::request* vr, size_t count);

	private:
		bool m_valid;
		int m_fd;

	private:
		context();
		context(const context&);
	};

	typedef mp::shared_ptr<context> shared_context;
	shared_context m_context;


	struct entry {
		shared_context context;
	};


	struct get_entry : entry {
		bool require_cas;
	};
	static void response_get(void* user, gw_get_response& res);


	struct get_multi_entry : entry {
		bool require_cas;
		size_t offset;
		unsigned *count;
		struct iovec*  vhead;
		wavy::request* rhead;
		size_t veclen;
	};
	static void response_get_multi(void* user, gw_get_response& res);


	struct set_entry : entry {
	};
	static void response_set(void* user, gw_set_response& res);


	struct delete_entry : entry {
	};
	static void response_delete(void* user, gw_delete_response& res);

	static void response_noreply_set(void* user, gw_set_response& res);
	static void response_noreply_delete(void* user, gw_delete_response& res);

private:
	Connection();
	Connection(const Connection&);
};


Memtext::Connection::Connection(int fd) :
	mp::wavy::handler(fd),
	m_buffer(MEMTEXT_INITIAL_ALLOCATION_SIZE),
	m_off(0),
	m_context(new context(fd))
{
	int (*cmd_get)(void*, memtext_command, memtext_request_retrieval*) =
		&mp::object_callback<int (memtext_command, memtext_request_retrieval*)>::
				mem_fun<Connection, &Connection::request_get>;

	int (*cmd_gets)(void*, memtext_command, memtext_request_retrieval*) =
		&mp::object_callback<int (memtext_command, memtext_request_retrieval*)>::
				mem_fun<Connection, &Connection::request_gets>;

	int (*cmd_set)(void*, memtext_command, memtext_request_storage*) =
		&mp::object_callback<int (memtext_command, memtext_request_storage*)>::
				mem_fun<Connection, &Connection::request_set>;

	int (*cmd_delete)(void*, memtext_command, memtext_request_delete*) =
		&mp::object_callback<int (memtext_command, memtext_request_delete*)>::
				mem_fun<Connection, &Connection::request_delete>;

	memtext_callback cb = {
		cmd_get,      // get
		cmd_gets,     // gets
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

Memtext::Connection::~Connection()
{
	m_context->invalidate();
}


inline int Memtext::Connection::context::is_valid() const
{
	return m_valid;
}

inline void Memtext::Connection::context::invalidate()
{
	m_valid = true;
}


Memtext::Connection::context::context(int fd) :
	m_valid(true), m_fd(fd) { }

Memtext::Connection::context::~context() { }


inline void Memtext::Connection::context::send_data(
		const char* buf, size_t buflen)
{
	wavy::write(m_fd, buf, buflen);
}

inline void Memtext::Connection::context::send_datav(
		struct iovec* vb, size_t count, shared_zone& life)
{
	wavy::request req(&mp::object_delete<shared_zone>, new shared_zone(life));
	wavy::writev(m_fd, vb, count, req);
}

inline void Memtext::Connection::context::send_datav(
		struct iovec* vb, wavy::request* vr, size_t count)
{
	wavy::writev(m_fd, vb, vr, count);
}


void Memtext::Connection::read_event()
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

// "VALUE "+keylen+" 0 "+uint32+" "+uint64+"\r\n\0"
#define HEADER_SIZE(keylen) \
		6 +(keylen)+ 3  +  10  + 1 +  20  +   3


int Memtext::Connection::request_get(
		memtext_command cmd,
		memtext_request_retrieval* r)
{
	return request_get_real(cmd, r, false);
}

int Memtext::Connection::request_gets(
		memtext_command cmd,
		memtext_request_retrieval* r)
{
	return request_get_real(cmd, r, true);
}

int Memtext::Connection::request_get_real(
		memtext_command cmd,
		memtext_request_retrieval* r,
		bool require_cas)
{
	LOG_TRACE("get");
	RELEASE_REFERENCE(life);

	if(r->key_num == 1) {
		const char* const key = r->key[0];
		size_t const key_len  = r->key_len[0];

		get_entry* e = life->allocate<get_entry>();
		e->context     = m_context;
		e->require_cas = require_cas;

		gw_get_request req;
		req.keylen   = key_len;
		req.key      = key;
		req.hash     = gateway::stdhash(req.key, req.keylen);
		req.life     = life;
		req.user     = reinterpret_cast<void*>(e);
		req.callback = &Connection::response_get;
	
		gateway::submit(req);

	} else {
		get_multi_entry* me[r->key_num];

		size_t const veclen = r->key_num * 2 + 1;  // +1: \r\nEND\r\n

		struct iovec*  vhead = (struct iovec* )life->malloc(
				sizeof(struct iovec )*veclen);

		wavy::request* rhead = (wavy::request*)life->malloc(
				sizeof(wavy::request)*veclen);

		unsigned* count      = (unsigned*)life->malloc(sizeof(unsigned));
		*count = r->key_num;

		memset(vhead, 0, sizeof(struct iovec )*(veclen-1));
		vhead[veclen-1].iov_base = const_cast<char*>("\r\nEND\r\n");
		vhead[veclen-1].iov_len  = 7;

		memset(rhead, 0, sizeof(wavy::request)*veclen);

		for(unsigned i=0; i < r->key_num; ++i) {
			me[i] = life->allocate<get_multi_entry>();
			me[i]->context     = m_context;
			me[i]->require_cas = require_cas;
			me[i]->offset      = i*2;
			me[i]->count       = count;
			me[i]->vhead       = vhead;
			me[i]->rhead       = rhead;
			me[i]->veclen      = veclen;
		}

		gw_get_request req;
		req.callback = &Connection::response_get_multi;
		req.life     = life;

		for(unsigned i=0; i < r->key_num; ++i) {
			// don't use life. msgpack::allocate is not thread-safe.
			req.user     = reinterpret_cast<void*>(me[i]);
			req.keylen   = r->key_len[i];
			req.key      = r->key[i];
			req.hash     = gateway::stdhash(req.key, req.keylen);
			gateway::submit(req);
		}
	}

	return 0;
}


int Memtext::Connection::request_set(
		memtext_command cmd,
		memtext_request_storage* r)
{
	LOG_TRACE("set");
	RELEASE_REFERENCE(life);

	if(r->flags || r->exptime) {
		wavy::write(fd(), NOT_SUPPORTED_REPLY, strlen(NOT_SUPPORTED_REPLY));
		return 0;
	}

	set_entry* e = life->allocate<set_entry>();
	e->context = m_context;

	gw_set_request req;
	req.keylen   = r->key_len;
	req.key      = r->key;
	req.vallen   = r->data_len;
	req.val      = r->data;
	req.hash     = gateway::stdhash(req.key, req.keylen);
	req.life     = life;
	req.user     = reinterpret_cast<void*>(e);
	if(r->noreply) {
		req.callback = &Connection::response_noreply_set;
	} else {
		req.callback = &Connection::response_set;
	}

	gateway::submit(req);

	return 0;
}


int Memtext::Connection::request_delete(
		memtext_command cmd,
		memtext_request_delete* r)
{
	LOG_TRACE("delete");
	RELEASE_REFERENCE(life);

	if(r->exptime) {
		wavy::write(fd(), NOT_SUPPORTED_REPLY, strlen(NOT_SUPPORTED_REPLY));
		return 0;
	}

	delete_entry* e = life->allocate<delete_entry>();
	e->context = m_context;

	gw_delete_request req;
	req.key      = r->key;
	req.keylen   = r->key_len;
	req.hash     = gateway::stdhash(req.key, req.keylen);
	req.life     = life;
	req.user     = reinterpret_cast<void*>(e);
	if(r->noreply) {
		req.callback = &Connection::response_noreply_delete;
	} else {
		req.callback = &Connection::response_delete;
	}

	gateway::submit(req);

	return 0;
}



void Memtext::Connection::response_get(void* user, gw_get_response& res)
{
	get_entry* e = reinterpret_cast<get_entry*>(user);
	context* const c = e->context.get();
	if(!c->is_valid()) { return; }

	LOG_TRACE("get response");

	if(res.error) {
		c->send_data(GET_FAILED_REPLY, strlen(GET_FAILED_REPLY));
		return;
	}

	if(!res.val) {
		c->send_data("END\r\n", 5);
		return;
	}

	char* const header = (char*)res.life->malloc(HEADER_SIZE(res.keylen));
	char* p = header;

	memcpy(p, "VALUE ", 6);           p += 6;
	memcpy(p, res.key,  res.keylen);  p += res.keylen;
	p += sprintf(p, " 0 %"PRIu32, res.vallen);

	if(e->require_cas) {
		p += sprintf(p, " %"PRIu64"\r\n", (uint64_t)0);
	} else {
		p[0] = '\r'; p[1] = '\n'; p += 2;
	}

	struct iovec vb[3];
	vb[0].iov_base = header;
	vb[0].iov_len  = p - header;
	vb[1].iov_base = const_cast<char*>(res.val);
	vb[1].iov_len  = res.vallen;
	vb[2].iov_base = const_cast<char*>("\r\nEND\r\n");
	vb[2].iov_len  = 7;

	c->send_datav(vb, 3, res.life);
}


void Memtext::Connection::response_get_multi(void* user, gw_get_response& res)
{
	get_multi_entry* e = reinterpret_cast<get_multi_entry*>(user);
	context* const c = e->context.get();
	if(!c->is_valid()) { return; }

	if(res.error || !res.val) {
		goto filled;
	}

	{
		// res.life is different from req.life and res.life includes req.life
		char* const header = (char*)res.life->malloc(HEADER_SIZE(res.keylen)+2);  // +2: \r\n
		char* p = header;
	
		memcpy(p, "\r\nVALUE ", 8);       p += 8;
		memcpy(p, res.key,  res.keylen);  p += res.keylen;
		p += sprintf(p, " 0 %"PRIu32, res.vallen);
	
		if(e->require_cas) {
			p += sprintf(p, " %"PRIu64"\r\n", (uint64_t)0);  // FIXME p64X casval
		} else {
			p[0] = '\r'; p[1] = '\n'; p += 2;
		}
	
		struct iovec*  vb = &e->vhead[e->offset];
		wavy::request* vr = &e->rhead[e->offset];
	
		vb[0].iov_base = header;
		vb[0].iov_len  = p - header;
		vb[1].iov_base = res.val;
		vb[1].iov_len  = res.vallen;
	
		vr[1] = wavy::request(&mp::object_delete<shared_zone>,
				new shared_zone(res.life));
	}

filled:
	if(__sync_sub_and_fetch(e->count, 1) == 0) {
		for(struct iovec* v=e->vhead, * const ve = e->vhead+e->veclen;
				v < ve; v+=2) {
			if(v->iov_base) {
				v->iov_base = (char*)v->iov_base + 2;  // strip \r\n
				v->iov_len -= 2;
				break;
			}
		}
		c->send_datav(e->vhead, e->rhead, e->veclen);
	}
}


void Memtext::Connection::response_set(void* user, gw_set_response& res)
{
	set_entry* e = reinterpret_cast<set_entry*>(user);
	context* const c = e->context.get();
	if(!c->is_valid()) { return; }

	LOG_TRACE("set response");

	if(res.error) {
		c->send_data(STORE_FAILED_REPLY, strlen(STORE_FAILED_REPLY));
		return;
	}

	c->send_data("STORED\r\n", 8);
}


void Memtext::Connection::response_delete(void* user, gw_delete_response& res)
{
	delete_entry* e = reinterpret_cast<delete_entry*>(user);
	context* const c = e->context.get();
	if(!c->is_valid()) { return; }

	LOG_TRACE("delete response");

	if(res.error) {
		c->send_data(DELETE_FAILED_REPLY, strlen(DELETE_FAILED_REPLY));
		return;
	}

	if(res.deleted) {
		c->send_data("DELETED\r\n", 9);
	} else {
		c->send_data("NOT FOUND\r\n", 11);
	}
}


void Memtext::Connection::response_noreply_set(void* user, gw_set_response& res)
{ }

void Memtext::Connection::response_noreply_delete(void* user, gw_delete_response& res)
{ }


}  // namespace kumo

