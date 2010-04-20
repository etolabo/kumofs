//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#define __STDC_LIMIT_MACROS
#define __STDC_FORMAT_MACROS
#include "gate/memcache_text.h"
#include "gate/memproto/memtext.h"
#include "log/mlogger.h"
#include "rpc/exception.h"
#include <mp/pthread.h>
#include <mp/stream_buffer.h>
#include <mp/object_callback.h>
#include <stdexcept>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <algorithm>
#include <memory>
#include <inttypes.h>
#include "config.h"  // PACKAGE VERSION

namespace kumo {
namespace {

static bool g_save_flag = false;

using gate::wavy;
using gate::shared_zone;
using gate::auto_zone;

static const size_t MEMTEXT_INITIAL_ALLOCATION_SIZE = 32*1024;
static const size_t MEMTEXT_RESERVE_SIZE = 4*1024;

class handler : public wavy::handler {
public:
	handler(int fd);
	~handler();

public:
	void read_event();

public:
	typedef mp::shared_ptr<bool> shared_valid;

	class context {
	public:
		context(int fd, mp::stream_buffer* buf, const shared_valid& valid);
		~context();

	public:
		int fd() const { return m_fd; }
		msgpack::zone* release_reference();
		const shared_valid& valid() { return m_valid; }

	private:
		int m_fd;
		mp::stream_buffer* m_buffer;
		shared_valid m_valid;

	private:
		context();
		context(const context&);
	};

private:
	mp::stream_buffer m_buffer;
	memtext_parser m_memproto;
	size_t m_off;
	shared_valid m_valid;

	context m_context;

private:
	handler();
	handler(const handler&);
};


struct entry {
	int fd;
	handler::shared_valid valid;
};

struct get_entry : entry {
	bool require_cas;
};

struct get_multi_entry : entry {
	bool require_cas;
	size_t offset;
	unsigned *count;
	struct iovec*  vhead;
	wavy::request* rhead;
	size_t veclen;
};

struct set_entry : entry {
};

struct delete_entry : entry {
};


inline void send_data(entry* e,
		const char* buf, size_t buflen)
{
	if(!*e->valid) { return; }
	wavy::write(e->fd, buf, buflen);
}

inline void send_datav(entry* e,
		struct iovec* vb, size_t count, auto_zone z)
{
	if(!*e->valid) { return; }
	wavy::request req(&mp::object_delete<msgpack::zone>, z.release());
	wavy::writev(e->fd, vb, count, req);
}

inline void send_datav(entry* e,
		struct iovec* vb, wavy::request* vr, size_t count)
{
	if(!*e->valid) { return; }
	wavy::writev(e->fd, vb, vr, count);
}


static const char* const NOT_SUPPORTED_REPLY = "CLIENT_ERROR supported\r\n";
static const char* const GET_FAILED_REPLY    = "SERVER_ERROR get failed\r\n";
static const char* const STORE_FAILED_REPLY  = "SERVER_ERROR store failed\r\n";
static const char* const DELETE_FAILED_REPLY = "SERVER_ERROR delete failed\r\n";
static const char* const VERSION_REPLY       = "VERSION " PACKAGE "-" VERSION "\r\n";

// "VALUE "+keylen+" "+uint16+" "+uint32+" "+uint64+"\r\n\0"
#define HEADER_SIZE(keylen) \
		6 +(keylen)+ 1+   5  + 1 +  10  + 1 +  20  +   3


void response_get(void* user,
		gate::res_get& res, auto_zone z)
{
	get_entry* e = reinterpret_cast<get_entry*>(user);
	LOG_TRACE("get response");

	if(res.error) {
		send_data(e, GET_FAILED_REPLY, strlen(GET_FAILED_REPLY));
		return;
	}

	if(!res.val || (g_save_flag && res.vallen < 2)) {
		send_data(e, "END\r\n", 5);
		return;
	}

	char* const header = (char*)z->malloc(HEADER_SIZE(res.keylen));
	char* p = header;

	memcpy(p, "VALUE ", 6);           p += 6;
	memcpy(p, res.key,  res.keylen);  p += res.keylen;
	if(g_save_flag) {
		union {
			uint16_t num;
			char mem[2];
		} cast;
		memcpy(cast.mem, res.val, 2);
		uint16_t flags = ntohs(cast.num);
		res.val    += 2;
		res.vallen -= 2;
		p += sprintf(p, " %"PRIu16" %"PRIu32, flags, res.vallen);
	} else {
		p += sprintf(p, " 0 %"PRIu32, res.vallen);
	}

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

	send_datav(e, vb, 3, z);
}

void response_get_multi(void* user,
		gate::res_get& res, auto_zone z)
{
	get_multi_entry* e = reinterpret_cast<get_multi_entry*>(user);
	LOG_TRACE("get multi response");

	if(res.error || !res.val || (g_save_flag && res.vallen < 2)) {
		goto filled;
	}

	{
		// res.life is different from req.life and res.life includes req.life
		char* const header = (char*)z->malloc(HEADER_SIZE(res.keylen)+2);  // +2: \r\n
		char* p = header;
	
		memcpy(p, "\r\nVALUE ", 8);       p += 8;
		memcpy(p, res.key,  res.keylen);  p += res.keylen;
		if(g_save_flag) {
			union {
				uint16_t num;
				char mem[2];
			} cast;
			memcpy(cast.mem, res.val, 2);
			uint16_t flags = ntohs(cast.num);
			res.val    += 2;
			res.vallen -= 2;
			p += sprintf(p, " %"PRIu16" %"PRIu32, flags, res.vallen);
		} else {
			p += sprintf(p, " 0 %"PRIu32, res.vallen);
		}
	
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
	
		vr[1] = wavy::request(&mp::object_delete<msgpack::zone>, z.release());
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
		send_datav(e, e->vhead, e->rhead, e->veclen);
	}
}

void response_set(void* user,
		gate::res_set& res, auto_zone z)
{
	set_entry* e = reinterpret_cast<set_entry*>(user);
	LOG_TRACE("set response");

	if(res.error) {
		send_data(e, STORE_FAILED_REPLY, strlen(STORE_FAILED_REPLY));
		return;
	}

	send_data(e, "STORED\r\n", 8);
}

void response_delete(void* user,
		gate::res_delete& res, auto_zone z)
{
	delete_entry* e = reinterpret_cast<delete_entry*>(user);
	LOG_TRACE("delete response");

	if(res.error) {
		send_data(e, DELETE_FAILED_REPLY, strlen(DELETE_FAILED_REPLY));
		return;
	}

	if(res.deleted) {
		send_data(e, "DELETED\r\n", 9);
	} else {
		send_data(e, "NOT FOUND\r\n", 11);
	}
}

void response_noreply_set(void* user,
		gate::res_set& res, auto_zone z)
{ }

void response_noreply_delete(void* user,
		gate::res_delete& res, auto_zone z)
{ }


#define RELEASE_REFERENCE(user, ctx, life) \
	handler::context* ctx = static_cast<handler::context*>(user); \
	shared_zone life(ctx->release_reference());


int request_get_single(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r,
		bool require_cas)
{
	LOG_TRACE("get");
	RELEASE_REFERENCE(user, ctx, life);

	const char* const key = r->key[0];
	size_t const key_len  = r->key_len[0];

	get_entry* e = life->allocate<get_entry>();
	e->fd          = ctx->fd();
	e->valid       = ctx->valid();
	e->require_cas = require_cas;

	gate::req_get req;
	req.keylen   = key_len;
	req.key      = key;
	req.hash     = gate::stdhash(req.key, req.keylen);
	req.user     = reinterpret_cast<void*>(e);
	req.callback = &response_get;
	req.life     = life;

	req.submit();
	return 0;
}

int request_get_multi(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r,
		bool require_cas)
{
	LOG_TRACE("get multi");
	RELEASE_REFERENCE(user, ctx, life);

	size_t const veclen = r->key_num * 2 + 1;  // +1: \r\nEND\r\n

	struct iovec*  vhead = (struct iovec* )life->malloc(
			sizeof(struct iovec )*veclen);

	wavy::request* rhead = (wavy::request*)life->malloc(
			sizeof(wavy::request)*veclen);

	unsigned* share_count = (unsigned*)life->malloc(sizeof(unsigned));

	*share_count = r->key_num;

	memset(vhead, 0, sizeof(struct iovec )*(veclen-1));
	vhead[veclen-1].iov_base = const_cast<char*>("\r\nEND\r\n");
	vhead[veclen-1].iov_len  = 7;

	memset(rhead, 0, sizeof(wavy::request)*veclen);


	get_multi_entry* me[r->key_num];
	for(unsigned i=0; i < r->key_num; ++i) {
		me[i] = life->allocate<get_multi_entry>();
		me[i]->fd          = ctx->fd();
		me[i]->valid       = ctx->valid();
		me[i]->require_cas = require_cas;
		me[i]->offset      = i*2;
		me[i]->count       = share_count;
		me[i]->vhead       = vhead;
		me[i]->rhead       = rhead;
		me[i]->veclen      = veclen;
	}

	for(unsigned i=0; i < r->key_num; ++i) {
		gate::req_get req;
		req.keylen   = r->key_len[i];
		req.key      = r->key[i];
		req.user     = reinterpret_cast<void*>(me[i]);
		req.hash     = gate::stdhash(req.key, req.keylen);
		req.callback = &response_get_multi;
		req.life     = life;

		req.submit();
	}

	return 0;
}

int request_get(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r)
{
	if(r->key_num == 1) {
		return request_get_single(user, cmd, r, false);
	} else {
		return request_get_multi(user, cmd, r, false);
	}
}

int request_gets(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r)
{
	if(r->key_num == 1) {
		return request_get_single(user, cmd, r, true);
	} else {
		return request_get_multi(user, cmd, r, true);
	}
}

int request_set(void* user,
		memtext_command cmd,
		memtext_request_storage* r)
{
	LOG_TRACE("set");
	RELEASE_REFERENCE(user, ctx, life);

	if((!g_save_flag && r->flags) || r->exptime) {
		wavy::write(ctx->fd(), NOT_SUPPORTED_REPLY, strlen(NOT_SUPPORTED_REPLY));
		return 0;
	}

	if(g_save_flag) {
		union {
			uint16_t num;
			char mem[2];
		} cast;
		cast.num = htons(r->flags);
		r->data     -= 2;
		r->data_len += 2;
		// テキストプロトコルでdataの前2バイトには\nとbytesが入っているが、
		// r->data_lenにコピーされている
		memcpy(const_cast<char*>(r->data), cast.mem, 2);
	}

	set_entry* e = life->allocate<set_entry>();
	e->fd    = ctx->fd();
	e->valid = ctx->valid();

	gate::req_set req;
	req.keylen   = r->key_len;
	req.key      = r->key;
	req.vallen   = r->data_len;
	req.val      = r->data;
	req.hash     = gate::stdhash(req.key, req.keylen);
	req.user     = reinterpret_cast<void*>(e);
	if(r->noreply) {
		req.async    = true;
		req.callback = &response_noreply_set;
	} else {
		req.callback = &response_set;
	}
	req.life     = life;

	req.submit();
	return 0;
}

int request_delete(void* user,
		memtext_command cmd,
		memtext_request_delete* r)
{
	LOG_TRACE("delete");
	RELEASE_REFERENCE(user, ctx, life);

	if(r->exptime) {
		wavy::write(ctx->fd(), NOT_SUPPORTED_REPLY, strlen(NOT_SUPPORTED_REPLY));
		return 0;
	}

	delete_entry* e = life->allocate<delete_entry>();
	e->fd    = ctx->fd();
	e->valid = ctx->valid();

	gate::req_delete req;
	req.key      = r->key;
	req.keylen   = r->key_len;
	req.hash     = gate::stdhash(req.key, req.keylen);
	req.user     = reinterpret_cast<void*>(e);
	if(r->noreply) {
		req.async    = true;
		req.callback = &response_noreply_delete;
	} else {
		req.callback = &response_delete;
	}
	req.life     = life;

	req.submit();
	return 0;
}

int request_version(void* user,
		memtext_command cmd,
		memtext_request_other* r)
{
	LOG_TRACE("version");
	RELEASE_REFERENCE(user, ctx, life);

	wavy::write(ctx->fd(), VERSION_REPLY, strlen(VERSION_REPLY));

	return 0;
}

handler::context::context(int fd, mp::stream_buffer* buf, const shared_valid& valid) :
	m_fd(fd), m_buffer(buf), m_valid(valid) { }

handler::context::~context() { }

msgpack::zone* handler::context::release_reference()
{
	auto_zone z(new msgpack::zone());

	mp::stream_buffer::reference* ref =
		z->allocate<mp::stream_buffer::reference>();
	m_buffer->release_to(ref);

	return z.release();
}


handler::handler(int fd) :
	wavy::handler(fd),
	m_buffer(MEMTEXT_INITIAL_ALLOCATION_SIZE),
	m_off(0),
	m_valid(new bool(true)),
	m_context(fd, &m_buffer, m_valid)
{
	memtext_callback cb = {
		request_get,    // get
		request_gets,   // gets
		request_set,    // set
		NULL,           // add
		NULL,           // replace
		NULL,           // append
		NULL,           // prepend
		NULL,           // cas
		request_delete, // delete
		NULL,           // incr
		NULL,           // decr
		request_version,// version
	};

	memtext_init(&m_memproto, &cb, &m_context);
}

handler::~handler()
{
	*m_valid = false;
}


void handler::read_event()
try {
	m_buffer.reserve_buffer(MEMTEXT_RESERVE_SIZE);

	ssize_t rl = ::read(fd(), m_buffer.buffer(), m_buffer.buffer_capacity());
	if(rl <= 0) {
		if(rl == 0) { throw rpc::connection_closed_error(); }
		if(errno == EAGAIN || errno == EINTR) { return; }
		else { throw rpc::connection_broken_error(); }
	}

	m_buffer.buffer_consumed(rl);

	do {
		int ret = memtext_execute(&m_memproto,
				(char*)m_buffer.data(), m_buffer.data_size(), &m_off);
		if(ret < 0) {
			throw std::runtime_error("parse error");
		} else if(ret == 0) {
			return;
		}
		m_buffer.data_used(m_off);
		m_off = 0;
	} while(m_buffer.data_size() > 0);

} catch(rpc::connection_error& e) {
	LOG_DEBUG(e.what());
	throw;
} catch (std::exception& e) {
	LOG_DEBUG("memcached text protocol error: ",e.what());
	throw;
} catch (...) {
	LOG_DEBUG("memcached text protocol error: unknown error");
	throw;
}


void accepted(int fd, int err)
{
	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(err));
		gate::fatal_stop();
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
	LOG_DEBUG("accept MemcacheText gate fd=",fd);
	wavy::add<handler>(fd);
}


}  // noname namespace


MemcacheText::MemcacheText(int lsock, bool save_flag) :
	m_lsock(lsock)
{
	g_save_flag = save_flag;
}

MemcacheText::~MemcacheText() {}

void MemcacheText::run()
{
	using namespace mp::placeholders;
	wavy::listen(m_lsock,
			mp::bind(&accepted, _1, _2));
}


}  // namespace kumo

