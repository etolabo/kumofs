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
#include "gate/cloudy.h"
#include "gate/memproto/memproto.h"
#include "log/mlogger.h"
#include "rpc/exception.h"
#include <mp/object_callback.h>
#include <mp/stream_buffer.h>
#include <stdexcept>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <algorithm>
#include <memory>

namespace kumo {
namespace {

using gate::wavy;
using gate::shared_zone;
using gate::auto_zone;

static const size_t CLOUDY_INITIAL_ALLOCATION_SIZE = 32*1024;
static const size_t CLOUDY_RESERVE_SIZE = 4*1024;

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
	memproto_parser m_memproto;
	shared_valid m_valid;

	context m_context;

private:
	handler();
	handler(const handler&);
};


struct entry {
	int fd;
	handler::shared_valid valid;
	memproto_header header;
};

struct get_entry : entry {
	bool flag_key;    // getk, getkq
	bool flag_quiet;  // getq, getkq
};

struct set_entry : entry {
};

struct delete_entry : entry {
};


void pack_header(
		char* hbuf, uint16_t status, uint8_t op,
		uint16_t keylen, uint32_t vallen, uint8_t extralen,
		uint32_t opaque, uint64_t cas)
{
	hbuf[0] = 0x81;
	hbuf[1] = op;
	*(uint16_t*)&hbuf[2] = htons(keylen);
	hbuf[4] = extralen;
	hbuf[5] = 0x00;
	*(uint16_t*)&hbuf[6] = htons(status);
	*(uint32_t*)&hbuf[8] = htonl(vallen + keylen + extralen);
	*(uint32_t*)&hbuf[12] = htonl(opaque);
	*(uint32_t*)&hbuf[16] = htonl((uint32_t)(cas>>32));
	*(uint32_t*)&hbuf[20] = htonl((uint32_t)(cas&0xffffffff));
}

void send_response_nodata(
		entry* e, uint8_t status, uint64_t cas)
{
	char* header = (char*)::malloc(MEMPROTO_HEADER_SIZE);
	if(!header) {
		throw std::bad_alloc();
	}

	pack_header(header, status, e->header.opcode,
			0, 0, 0,
			e->header.opaque, cas);

	if(!*e->valid) { return; }
	wavy::request req(&::free, header);
	wavy::write(e->fd, header, MEMPROTO_HEADER_SIZE, req);
}

void send_response(
		entry* e, auto_zone z,
		uint8_t status,
		const char* key, uint16_t keylen,
		const void* val, uint32_t vallen,
		const char* extra, uint16_t extralen,
		uint64_t cas)
{
	char* header = (char*)z->malloc(MEMPROTO_HEADER_SIZE);
	pack_header(header, status, e->header.opcode,
			keylen, vallen, extralen,
			e->header.opaque, cas);

	struct iovec vb[4];

	vb[0].iov_base = header;
	vb[0].iov_len  = MEMPROTO_HEADER_SIZE;
	size_t cnt = 1;

	if(extralen > 0) {
		vb[cnt].iov_base = const_cast<char*>(extra);
		vb[cnt].iov_len  = extralen;
		++cnt;
	}

	if(keylen > 0) {
		vb[cnt].iov_base = const_cast<char*>(key);
		vb[cnt].iov_len  = keylen;
		++cnt;
	}

	if(vallen > 0) {
		vb[cnt].iov_base = const_cast<void*>(val);
		vb[cnt].iov_len  = vallen;
		++cnt;
	}

	if(!*e->valid) { return; }
	wavy::request req(&mp::object_delete<msgpack::zone>, z.release());
	wavy::writev(e->fd, vb, cnt, req);
}


#define RELEASE_REFERENCE(user, fd, life) \
	handler::context* ctx = static_cast<handler::context*>(user); \
	shared_zone life(ctx->release_reference());


static const uint32_t ZERO_FLAG = 0;

void response_getx(void* user,
		gate::res_get& res, auto_zone z)
{
	get_entry* e = static_cast<get_entry*>(user);
	LOG_TRACE("get response");

	if(res.error) {
		// error
		LOG_TRACE("getx res err");
		if(e->flag_quiet) { return; }
		send_response_nodata(e, MEMPROTO_RES_INVALID_ARGUMENTS, 0);
		return;
	}

	if(!res.val) {
		// not found
		if(e->flag_quiet) { return; }
		send_response_nodata(e, MEMPROTO_RES_KEY_NOT_FOUND, 0);
		return;
	}

	// found
	send_response(e, z, MEMPROTO_RES_NO_ERROR,
			res.key, (e->flag_key ? res.keylen : 0),
			res.val, res.vallen,
			(char*)&ZERO_FLAG, 4,
			0);
}

void response_set(void* user,
		gate::res_set& res, auto_zone z)
{
	set_entry* e = static_cast<set_entry*>(user);
	LOG_TRACE("set response");

	if(res.error) {
		// error
		send_response_nodata(e, MEMPROTO_RES_OUT_OF_MEMORY, 0);
		return;
	}

	// stored
	send_response_nodata(e, MEMPROTO_RES_NO_ERROR, 0);
}

void response_delete(void* user,
		gate::res_delete& res, auto_zone z)
{
	delete_entry* e = static_cast<delete_entry*>(user);
	LOG_TRACE("delete response");

	if(res.error) {
		// error
		send_response_nodata(e, MEMPROTO_RES_INVALID_ARGUMENTS, 0);
		return;
	}

	if(res.deleted) {
		send_response_nodata(e, MEMPROTO_RES_NO_ERROR, 0);
	} else {
		send_response_nodata(e, MEMPROTO_RES_OUT_OF_MEMORY, 0);
	}
}


void request_getx(void* user,
		memproto_header* h, const char* key, uint16_t keylen)
{
	RELEASE_REFERENCE(user, ctx, life);
	LOG_TRACE("getx");

	get_entry* e = life->allocate<get_entry>();
	e->fd         = ctx->fd();
	e->valid      = ctx->valid();
	e->header     = *h;
	e->flag_key   = (h->opcode == MEMPROTO_CMD_GETK || h->opcode == MEMPROTO_CMD_GETKQ);
	e->flag_quiet = (h->opcode == MEMPROTO_CMD_GETQ || h->opcode == MEMPROTO_CMD_GETKQ);

	gate::req_get req;
	req.keylen   = keylen;
	req.key      = key;
	req.user     = static_cast<void*>(e);
	req.callback = &response_getx;
	req.life     = life;

	req.submit();
}

void request_set(void* user,
		memproto_header* h, const char* key, uint16_t keylen,
		const char* val, uint32_t vallen,
		uint32_t flags, uint32_t expiration)
{
	RELEASE_REFERENCE(user, ctx, life);
	LOG_TRACE("set");

	if(h->cas || flags || expiration) {
		// FIXME error response
		throw std::runtime_error("memcached binary protocol: invalid argument");
	}

	set_entry* e = life->allocate<set_entry>();
	e->fd         = ctx->fd();
	e->valid      = ctx->valid();
	e->header     = *h;

	gate::req_set req;
	req.keylen   = keylen;
	req.key      = key;
	req.vallen   = vallen;
	req.val      = val;
	req.user     = static_cast<void*>(e);
	req.callback = &response_set;
	req.life     = life;

	req.submit();
}

void request_delete(void* user,
		memproto_header* h, const char* key, uint16_t keylen,
		uint32_t expiration)
{
	RELEASE_REFERENCE(user, ctx, life);
	LOG_TRACE("delete");

	if(expiration) {
		// FIXME error response
		throw std::runtime_error("memcached binary protocol: invalid argument");
	}

	delete_entry* e = life->allocate<delete_entry>();
	e->fd         = ctx->fd();
	e->valid      = ctx->valid();
	e->header     = *h;

	gate::req_delete req;
	req.key      = key;
	req.keylen   = keylen;
	req.user     = static_cast<void*>(e);
	req.callback = &response_delete;
	req.life     = life;

	req.submit();
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
	m_buffer(CLOUDY_INITIAL_ALLOCATION_SIZE),
	m_valid(new bool(true)),
	m_context(fd, &m_buffer, m_valid)
{
	memproto_callback cb = {
		request_getx,    // get
		request_set,     // set
		NULL,            // add
		NULL,            // replace
		request_delete,  // delete
		NULL,            // increment
		NULL,            // decrement
		NULL,            // quit
		NULL,            // flush
		request_getx,    // getq
		NULL,            // noop
		NULL,            // version
		request_getx,    // getk
		request_getx,    // getkq
		NULL,            // append
		NULL,            // prepend
	};

	memproto_parser_init(&m_memproto, &cb,
			static_cast<void*>(&m_context));
}

handler::~handler()
{
	*m_valid = false;
}


void handler::read_event()
try {
	m_buffer.reserve_buffer(CLOUDY_RESERVE_SIZE);

	size_t rl = ::read(fd(), m_buffer.buffer(), m_buffer.buffer_capacity());
	if(rl <= 0) {
		if(rl == 0) { throw rpc::connection_closed_error(); }
		if(errno == EAGAIN || errno == EINTR) { return; }
		else { throw rpc::connection_broken_error(); }
	}

	m_buffer.buffer_consumed(rl);

	do {
		size_t off = 0;
		int ret = memproto_parser_execute(&m_memproto,
				(char*)m_buffer.data(), m_buffer.data_size(), &off);

		if(ret == 0) {
			break;
		}

		if(ret < 0) {
			//std::cout << "parse error " << ret << std::endl;
			throw std::runtime_error("parse error");
		}

		m_buffer.data_used(off);

		ret = memproto_dispatch(&m_memproto);
		if(ret <= 0) {
			LOG_DEBUG("unknown command ",(uint16_t)-ret);
			throw std::runtime_error("unknown command");
		}

	} while(m_buffer.data_size() > 0);

} catch(rpc::connection_error& e) {
	LOG_DEBUG(e.what());
	throw;
} catch (std::exception& e) {
	LOG_DEBUG("memcached binary protocol error: ",e.what());
	throw;
} catch (...) {
	LOG_DEBUG("memcached binary protocol error: unknown error");
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
	LOG_DEBUG("accept Cloudy gate fd=",fd);
	wavy::add<handler>(fd);
}


}  // noname namespace


Cloudy::Cloudy(int lsock) :
	m_lsock(lsock) { }

Cloudy::~Cloudy() {}

void Cloudy::run()
{
	using namespace mp::placeholders;
	wavy::listen(m_lsock,
			mp::bind(&accepted, _1, _2));
}


}  // namespace kumo

