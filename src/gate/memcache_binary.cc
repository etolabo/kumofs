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
#include "gate/memcache_binary.h"
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
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include <memory>
#include <deque>

namespace kumo {
namespace {

static bool g_save_flag = false;
static bool g_save_exptime = false;
static uint32_t g_system_time;

#define RELATIVE_MAX (60*60*24*30)

uint32_t exptime_to_system(uint32_t exptime)
{
	if(exptime == 0) { return 0; }
	if(exptime <= RELATIVE_MAX) {
		return exptime + g_system_time;
	} else {
		return exptime;
	}
}

void update_system_time()
{
	struct timeval v;
	if(gettimeofday(&v, NULL) == 0) {
		g_system_time = v.tv_sec;
	}
}

using gate::wavy;
using gate::shared_zone;
using gate::auto_zone;

static const size_t MEMPROTO_INITIAL_ALLOCATION_SIZE = 32*1024;
static const size_t MEMPROTO_RESERVE_SIZE = 4*1024;

class handler : public wavy::handler {
public:
	handler(int fd);
	~handler();

public:
	void read_event();

private:
	// get, getq, getk, getkq
	void request_getx(memproto_header* h,
			const char* key, uint16_t keylen);

	// set
	void request_set(memproto_header* h,
			const char* key, uint16_t keylen,
			const char* val, uint32_t vallen,
			uint32_t flags, uint32_t expiration);

	// delete
	void request_delete(memproto_header* h,
			const char* key, uint16_t keylen,
			uint32_t expiration);

	// noop
	void request_noop(memproto_header* h);

	void request_flush(memproto_header* h,
			uint32_t expiration);

private:
	memproto_parser m_memproto;
	mp::stream_buffer m_buffer;

	struct entry;

	class response_queue {
	public:
		response_queue(int fd);
		~response_queue();

		void push_entry(entry* e);
		void reached_try_send(entry* e, auto_zone z,
				struct iovec* vec, size_t veclen);

		int is_valid() const;
		void invalidate();

	private:
		bool m_valid;
		int m_fd;

		struct element_t {
			element_t(entry* a) : e(a), vec(NULL), veclen(0) { }

			void filled() { e = NULL; }
			bool is_filled() const { return e == NULL; }

			entry* e;
			shared_zone life;
			struct iovec* vec;
			size_t veclen;
		};

		mp::pthread_mutex m_queue_mutex;

		typedef std::deque<element_t> queue_t;
		queue_t m_queue;

		struct find_entry_compare;

	private:
		response_queue();
		response_queue(const response_queue&);
	};

	typedef mp::shared_ptr<response_queue> shared_entry_queue;
	shared_entry_queue m_queue;


	struct entry {
		shared_entry_queue queue;
		memproto_header header;
	};


	// get, getq, getk, getkq
	struct get_entry : entry {
		bool flag_key;
		bool flag_quiet;
	};
	static void response_getx(void* user,
			gate::res_get& res, auto_zone z);


	// set
	struct set_entry : entry {
	};
	static void response_set(void* user,
			gate::res_set& res, auto_zone z);
	static void response_cas(void* user,
			gate::res_set& res, auto_zone z);


	// delete
	struct delete_entry : entry {
	};
	static void response_delete(void* user,
			gate::res_delete& res, auto_zone z);


	static void send_response_nosend(entry* e, auto_zone z);

	static void send_response_nodata(entry* e, auto_zone z,
			uint8_t status);

	static void send_response(entry* e, auto_zone z,
			uint8_t status,
			const char* key, uint16_t keylen,
			const void* val, uint32_t vallen,
			const char* extra, uint16_t extralen,
			uint64_t cas);

	static void pack_header(
			char* hbuf, uint16_t status, uint8_t op,
			uint16_t keylen, uint32_t vallen, uint8_t extralen,
			uint32_t opaque, uint64_t cas);

private:
	handler();
	handler(const handler&);
};


handler::response_queue::response_queue(int fd) :
	m_valid(true), m_fd(fd) { }

handler::response_queue::~response_queue() { }


inline int handler::response_queue::is_valid() const
{
	return m_valid;
}

inline void handler::response_queue::invalidate()
{
	m_valid = false;

	// untie circular reference.
	// m_queue -> element_t -> shared_zone -> {get,set,delete}_entry
	// -> m_queue.
	mp::pthread_scoped_lock mqlk(m_queue_mutex);
	m_queue.clear();
}


void handler::response_queue::push_entry(entry* e)
{
	element_t m(e);

	mp::pthread_scoped_lock mqlk(m_queue_mutex);
	m_queue.push_back(m);
}


struct handler::response_queue::find_entry_compare {
	find_entry_compare(entry* key) : m_key(key) { }

	bool operator() (const element_t& elem)
	{
		return elem.e == m_key;
	}

	entry* m_key;
};

void handler::response_queue::reached_try_send(
		entry* e, auto_zone z,
		struct iovec* vec, size_t veclen)
{
	mp::pthread_scoped_lock mqlk(m_queue_mutex);

	queue_t::iterator found = std::find_if(m_queue.begin(), m_queue.end(),
			find_entry_compare(e));

	if(found == m_queue.end()) {
		// invalidated
		return;
	}

	shared_zone life(z.release());

	found->life = life;
	found->vec = vec;
	found->veclen = veclen;
	found->filled();

	do {
		element_t& elem(m_queue.front());

		if(!elem.is_filled()) {
			break;
		}

		if(elem.veclen > 0) {
			std::auto_ptr<shared_zone> sz(new shared_zone(elem.life));
			wavy::request req(&mp::object_delete<shared_zone>, sz.get());
			wavy::writev(m_fd, elem.vec, elem.veclen, req);
			sz.release();
		}

		m_queue.pop_front();
	} while(!m_queue.empty());

#if 0
	size_t reqlen = 0;

	queue_t::iterator qlast = m_queue.begin();
	for(queue_t::const_iterator qend = m_queue.end(); qlast != qend; ++qlast) {
		if(qlast->e) { break; }
		reqlen += qlast->veclen;
	}

	if(reqlen == 0) { return; }

	// optimize
	//if(m_queue.begin() + 1 == qlast) {
	//}

	typedef mp::wavy::output::request wavy_request;

	struct iovec* const vb = (struct iovec*)found->life->malloc(
			sizeof(struct iovec) * reqlen);

	wavy_request* const vr = (wavy_request*)found->life->malloc(
			sizeof(wavy_request) * reqlen);

	memset(vr, 0, sizeof(wavy_request) * reqlen);


	struct iovec* vbp = vb;
	wavy_request* vrp = vr;

	for(queue_t::const_iterator q(m_queue.begin()); q != qlast; ++q) {
		memcpy(vbp, q->vec, sizeof(struct iovec) * q->veclen);

		vrp[q->veclen-1] = wavy_request(
				&mp::object_delete<shared_zone>,
				new shared_zone(q->life));

		vbp += q->veclen;
		vrp += q->veclen;
	}

	m_queue.erase(m_queue.begin(), qlast);

	mqlk.unlock();

	wavy::writev(m_fd, vb, vr, reqlen);
#endif
}


void handler::pack_header(
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
	*(uint32_t*)&hbuf[20] = htonl((uint32_t)(cas&0xffffffffULL));
}

inline void handler::send_response_nosend(entry* e, auto_zone z)
{
	e->queue->reached_try_send(e, z, NULL, 0);
}

void handler::send_response_nodata(
		entry* e, auto_zone z,
		uint8_t status)
{
	char* header = (char*)z->malloc(MEMPROTO_HEADER_SIZE);

	pack_header(header, status, e->header.opcode,
			0, 0, 0,
			e->header.opaque, 0);  // cas = 0

	struct iovec* vec = (struct iovec*)z->malloc(
			sizeof(struct iovec) * 1);
	vec[0].iov_base = header;
	vec[0].iov_len  = MEMPROTO_HEADER_SIZE;

	e->queue->reached_try_send(e, z, vec, 1);
}

void handler::send_response(
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

	struct iovec* vec = (struct iovec*)z->malloc(
			sizeof(struct iovec) * 4);

	vec[0].iov_base = header;
	vec[0].iov_len  = MEMPROTO_HEADER_SIZE;
	size_t cnt = 1;

	if(extralen > 0) {
		vec[cnt].iov_base = const_cast<char*>(extra);
		vec[cnt].iov_len  = extralen;
		++cnt;
	}

	if(keylen > 0) {
		vec[cnt].iov_base = const_cast<char*>(key);
		vec[cnt].iov_len  = keylen;
		++cnt;
	}

	if(vallen > 0) {
		vec[cnt].iov_base = const_cast<void*>(val);
		vec[cnt].iov_len  = vallen;
		++cnt;
	}

	e->queue->reached_try_send(e, z, vec, cnt);
}


handler::handler(int fd) :
	wavy::handler(fd),
	m_buffer(MEMPROTO_INITIAL_ALLOCATION_SIZE),
	m_queue(new response_queue(fd))
{
	void (*cmd_getx)(void*, memproto_header*,
			const char*, uint16_t) = &mp::object_callback<void (memproto_header*,
				const char*, uint16_t)>
				::mem_fun<handler, &handler::request_getx>;

	void (*cmd_set)(void*, memproto_header*,
			const char*, uint16_t,
			const char*, uint32_t,
			uint32_t, uint32_t) = &mp::object_callback<void (memproto_header*,
				const char*, uint16_t,
				const char*, uint32_t,
				uint32_t, uint32_t)>
				::mem_fun<handler, &handler::request_set>;

	void (*cmd_delete)(void*, memproto_header*,
			const char*, uint16_t,
			uint32_t) = &mp::object_callback<void (memproto_header*,
				const char*, uint16_t,
				uint32_t)>
				::mem_fun<handler, &handler::request_delete>;

	void (*cmd_noop)(void*, memproto_header*) =
			&mp::object_callback<void (memproto_header*)>
				::mem_fun<handler, &handler::request_noop>;

	void (*cmd_flush)(void*, memproto_header*,
			uint32_t) = &mp::object_callback<void (memproto_header*,
				uint32_t expiration)>
				::mem_fun<handler, &handler::request_flush>;

	memproto_callback cb = {
		cmd_getx,    // get
		cmd_set,     // set
		NULL,        // add
		cmd_set,     // replace
		cmd_delete,  // delete
		NULL,        // increment
		NULL,        // decrement
		NULL,        // quit
		cmd_flush,   // flush
		cmd_getx,    // getq
		cmd_noop,    // noop
		NULL,        // version
		cmd_getx,    // getk
		cmd_getx,    // getkq
		NULL,        // append
		NULL,        // prepend
	};

	memproto_parser_init(&m_memproto, &cb, this);
}

handler::~handler()
{
	m_queue->invalidate();
}


void handler::read_event()
try {
	m_buffer.reserve_buffer(MEMPROTO_RESERVE_SIZE);

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


#define RELEASE_REFERENCE(life) \
	shared_zone life(new msgpack::zone()); \
	{ \
		mp::stream_buffer::reference* ref = \
			life->allocate<mp::stream_buffer::reference>(); \
		m_buffer.release_to(ref); \
	}

#define RELEASE_REFERENCE_AUTO(z) \
	auto_zone z(new msgpack::zone()); \
	{ \
		mp::stream_buffer::reference* ref = \
			z->allocate<mp::stream_buffer::reference>(); \
		m_buffer.release_to(ref); \
	}


void handler::request_getx(memproto_header* h,
		const char* key, uint16_t keylen)
{
	LOG_TRACE("getx");
	RELEASE_REFERENCE(life);

	get_entry* e = life->allocate<get_entry>();
	e->queue      = m_queue;
	e->header     = *h;
	e->flag_key   = (h->opcode == MEMPROTO_CMD_GETK || h->opcode == MEMPROTO_CMD_GETKQ);
	e->flag_quiet = (h->opcode == MEMPROTO_CMD_GETQ || h->opcode == MEMPROTO_CMD_GETKQ);

	gate::req_get req;
	req.keylen   = keylen;
	req.key      = key;
	req.user     = reinterpret_cast<void*>(e);
	req.callback = &handler::response_getx;
	req.life     = life;

	m_queue->push_entry(e);
	req.submit();
}

void handler::request_set(memproto_header* h,
		const char* key, uint16_t keylen,
		const char* val, uint32_t vallen,
		uint32_t flags, uint32_t expiration)
{
	LOG_TRACE("set");
	RELEASE_REFERENCE(life);

	if(h->opcode == MEMPROTO_CMD_REPLACE && !h->cas) {
		// replace without cas value is not supported
		throw std::runtime_error("memcached binary protocol: unsupported requrest");
	}

	if((!g_save_flag && flags) || (!g_save_exptime && expiration)) {
		// FIXME error response
		throw std::runtime_error("memcached binary protocol: invalid argument");
	}

	if(g_save_flag || g_save_exptime) {
		// バイナリプロトコルでdataの前6バイトにはkey,extra,casが入っている
		// extraとcasは変数にコピーされているので、keyを動かす
		char* xkey = (char*)life->malloc(keylen);
		memcpy(xkey, key, keylen);
		key = xkey;
	}

	if(g_save_flag) {
		union {
			uint16_t num;
			char mem[2];
		} cast;
		cast.num = htons(flags);
		val    -= 2;
		vallen += 2;
		memcpy(const_cast<char*>(val), cast.mem, 2);
	}

	if(g_save_exptime) {
		union {
			uint32_t num;
			char mem[4];
		} cast;
		cast.num = htonl( exptime_to_system(expiration) );
		val    -= 4;
		vallen += 4;
		memcpy(const_cast<char*>(val), cast.mem, 4);
	}

	set_entry* e = life->allocate<set_entry>();
	e->queue      = m_queue;
	e->header     = *h;

	gate::req_set req;
	req.keylen   = keylen;
	req.key      = key;
	req.vallen   = vallen;
	req.val      = val;
	req.user     = reinterpret_cast<void*>(e);
	req.callback = &handler::response_set;
	req.life     = life;
	if(h->cas) {
		req.operation = gate::OP_CAS;
		req.clocktime = h->cas;
		req.callback = &handler::response_cas;
	}

	m_queue->push_entry(e);
	req.submit();
}

void handler::request_delete(memproto_header* h,
		const char* key, uint16_t keylen,
		uint32_t expiration)
{
	LOG_TRACE("delete");
	RELEASE_REFERENCE(life);

	if(expiration) {
		// FIXME error response
		throw std::runtime_error("memcached binary protocol: invalid argument");
	}

	delete_entry* e = life->allocate<delete_entry>();
	e->queue      = m_queue;
	e->header     = *h;

	gate::req_delete req;
	req.key      = key;
	req.keylen   = keylen;
	req.user     = reinterpret_cast<void*>(e);
	req.callback = &handler::response_delete;
	req.life     = life;

	m_queue->push_entry(e);
	req.submit();
}

void handler::request_noop(memproto_header* h)
{
	LOG_TRACE("noop");
	RELEASE_REFERENCE_AUTO(z);

	entry* e = z->allocate<entry>();
	e->queue      = m_queue;
	e->header     = *h;

	m_queue->push_entry(e);

	send_response_nodata(e, z, MEMPROTO_RES_NO_ERROR);
}

void handler::request_flush(memproto_header* h,
		uint32_t expiration)
{
	LOG_TRACE("flush");
	RELEASE_REFERENCE_AUTO(z);

	if(expiration) {
		// FIXME error response
		throw std::runtime_error("memcached binary protocol: invalid argument");
	}

	entry* e = z->allocate<entry>();
	e->queue      = m_queue;
	e->header     = *h;

	m_queue->push_entry(e);

	send_response_nodata(e, z, MEMPROTO_RES_NO_ERROR);
}


static const uint32_t ZERO_FLAG = 0;

void handler::response_getx(void* user,
		gate::res_get& res, auto_zone z)
{
	get_entry* e = reinterpret_cast<get_entry*>(user);
	if(!e->queue->is_valid()) { return; }

	LOG_TRACE("get response");

	if(res.error) {
		// error
		if(e->flag_quiet) {
			send_response_nosend(e, z);
			return;
		}
		LOG_TRACE("getx res err");
		send_response_nodata(e, z, MEMPROTO_RES_OUT_OF_MEMORY);
		return;
	}

	if(!res.val) {
		// not found
		if(e->flag_quiet) {
			send_response_nosend(e, z);
			return;
		}
		send_response_nodata(e, z, MEMPROTO_RES_KEY_NOT_FOUND);
		return;
	}

	if(g_save_exptime) {
		if(res.vallen < 4) {
			send_response_nodata(e, z, MEMPROTO_RES_KEY_NOT_FOUND);
			return;
		}
		union {
			uint32_t num;
			char mem[4];
		} cast;
		memcpy(cast.mem, res.val, 4);
		uint32_t exptime = ntohl(cast.num);
		if(exptime != 0 && exptime < g_system_time) {
			if(e->flag_quiet) {
				send_response_nosend(e, z);
				return;
			}
			send_response_nodata(e, z, MEMPROTO_RES_KEY_NOT_FOUND);
			return;
		}
		res.vallen -= 4;
		res.val    += 4;
	}

	char* flags = NULL;
	if(g_save_flag) {
		if(res.vallen < 2) {
			if(e->flag_quiet) {
				send_response_nosend(e, z);
				return;
			}
			send_response_nodata(e, z, MEMPROTO_RES_KEY_NOT_FOUND);
			return;
		}
		flags = (char*)z->malloc(4);
		memset(flags, 0, 4);
		memcpy(flags+2, res.val, 2);
		res.val    += 2;
		res.vallen -= 2;
	}

	// found
	if(flags) {
		send_response(e, z, MEMPROTO_RES_NO_ERROR,
				res.key, (e->flag_key ? res.keylen : 0),
				res.val, res.vallen,
				flags, 4,
				res.clocktime);
	} else {
		send_response(e, z, MEMPROTO_RES_NO_ERROR,
				res.key, (e->flag_key ? res.keylen : 0),
				res.val, res.vallen,
				(char*)&ZERO_FLAG, 4,
				res.clocktime);
	}
}

void handler::response_set(void* user,
		gate::res_set& res, auto_zone z)
{
	set_entry* e = reinterpret_cast<set_entry*>(user);
	if(!e->queue->is_valid()) { return; }

	LOG_TRACE("set response");

	if(res.error) {
		// error
		send_response_nodata(e, z, MEMPROTO_RES_OUT_OF_MEMORY);
		return;
	}

	// stored
	send_response_nodata(e, z, MEMPROTO_RES_NO_ERROR);
}

void handler::response_cas(void* user,
		gate::res_set& res, auto_zone z)
{
	set_entry* e = reinterpret_cast<set_entry*>(user);
	if(!e->queue->is_valid()) { return; }

	LOG_TRACE("cas response");

	if(res.error) {
		// error
		send_response_nodata(e, z, MEMPROTO_RES_OUT_OF_MEMORY);
		return;
	}

	if(!res.cas_success) {
		// FIXME EXISTS, NOT_FOUND
		send_response_nodata(e, z, MEMPROTO_RES_KEY_EXISTS);
		return;
	}

	// stored
	send_response_nodata(e, z, MEMPROTO_RES_NO_ERROR);
}

void handler::response_delete(void* user,
		gate::res_delete& res, auto_zone z)
{
	delete_entry* e = reinterpret_cast<delete_entry*>(user);
	if(!e->queue->is_valid()) { return; }

	LOG_TRACE("delete response");

	if(res.error) {
		// error
		send_response_nodata(e, z, MEMPROTO_RES_INVALID_ARGUMENTS);
		return;
	}

	if(res.deleted) {
		send_response_nodata(e, z, MEMPROTO_RES_NO_ERROR);
	} else {
		send_response_nodata(e, z, MEMPROTO_RES_OUT_OF_MEMORY);
	}
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
	LOG_DEBUG("accept MemcacheBinary gate fd=",fd);
	wavy::add<handler>(fd);
}


}  // noname namespace


MemcacheBinary::MemcacheBinary(int lsock, bool save_flag, bool save_exptime) :
	m_lsock(lsock)
{
	g_save_flag = save_flag;
	g_save_exptime = save_exptime;
}


MemcacheBinary::~MemcacheBinary() {}

void MemcacheBinary::run()
{
	using namespace mp::placeholders;
	wavy::listen(m_lsock,
			mp::bind(&accepted, _1, _2));

	if(g_save_exptime) {
		update_system_time();
		struct timespec interval = {0, 500*1000*1000};
		wavy::timer(&interval, &update_system_time);
	}
}


}  // namespace kumo

