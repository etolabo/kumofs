#include "gateway/gate_cloudy.h"
#include "gateway/memproto/memproto.h"
#include "log/mlogger.h"
#include "gateway/framework.h"  // FIXME net->signal_end()
#include <mp/object_callback.h>
#include <mp/stream_buffer.h>
#include <stdexcept>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <algorithm>
#include <memory>

namespace kumo {


namespace {
	class handler;
}


Cloudy::Cloudy(int lsock) :
	m_lsock(lsock) { }

Cloudy::~Cloudy() {}


void Cloudy::accepted(int fd, int err)
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
	wavy::add<handler>(fd);
}

void Cloudy::listen()
{
	using namespace mp::placeholders;
	wavy::listen(m_lsock,
			mp::bind(&Cloudy::accepted, _1, _2));
}


namespace {

static const size_t CLOUDY_INITIAL_ALLOCATION_SIZE = 32*1024;
static const size_t CLOUDY_RESERVE_SIZE = 4*1024;

class handler : public wavy::handler {
public:
	handler(int fd);
	~handler();

public:
	void read_event();

private:
	// get, getq, getk, getkq
	void memproto_getx(memproto_header* h, const char* key, uint16_t keylen);

	// set
	void memproto_set(memproto_header* h, const char* key, uint16_t keylen,
			const char* val, uint32_t vallen,
			uint32_t flags, uint32_t expiration);

	// delete
	void memproto_delete(memproto_header* h, const char* key, uint16_t keylen,
			uint32_t expiration);

private:
	memproto_parser m_memproto;
	mp::stream_buffer m_buffer;

	typedef gateway::get_request get_request;
	typedef gateway::set_request set_request;
	typedef gateway::delete_request delete_request;

	typedef gateway::get_response get_response;
	typedef gateway::set_response set_response;
	typedef gateway::delete_response delete_response;

	typedef rpc::shared_zone shared_zone;

	shared_zone m_zone;

	struct Queue {
		Queue() : m_valid(true) { }
		~Queue() { }
		int is_valid() const { return m_valid; }
		void invalidate() { m_valid = false; }
	private:
		bool m_valid;
	};

	typedef mp::shared_ptr<Queue> SharedQueue;
	SharedQueue m_queue;


	struct Responder {
		Responder(memproto_header* h, int fd, SharedQueue& queue) :
			m_fd(fd), m_h(*h), m_queue(queue) { }
		~Responder() { }

		bool is_valid() const { return m_queue->is_valid(); }

		int fd() const { return m_fd; }

	protected:
		void send_response_nodata(
				uint8_t status, uint64_t cas);

		void send_response(
				shared_zone& life,
				uint8_t status,
				const char* key, uint16_t keylen,
				const void* val, uint16_t vallen,
				const char* extra, uint16_t extralen,
				uint64_t cas);

	private:
		static void pack_header(
				char* hbuf, uint16_t status, uint8_t op,
				uint16_t keylen, uint32_t vallen, uint8_t extralen,
				uint32_t opaque, uint64_t cas);

	private:
		int m_fd;
		memproto_header m_h;
		SharedQueue m_queue;
	};

	struct ResGet : Responder {
		ResGet(memproto_header* h, int fd, SharedQueue& queue) :
			Responder(h, fd, queue) { }
		~ResGet() { }
		void response(get_response& res);
	public:
		void set_req_key() { m_req_key = true; }
		void set_req_quiet() { m_req_quiet = true; }
	private:
		bool m_req_key;
		bool m_req_quiet;
	};

	struct ResSet : Responder {
		ResSet(memproto_header* h, int fd, SharedQueue& queue) :
			Responder(h, fd, queue) { }
		~ResSet() { }
		void response(set_response& res);
		void no_response(set_response& res);
	};

	struct ResDelete : Responder {
		ResDelete(memproto_header* h, int fd, SharedQueue& queue) :
			Responder(h, fd, queue) { }
		~ResDelete() { }
		void response(delete_response& res);
		void no_response(delete_response& res);
	};

private:
	handler();
	handler(const handler&);
};


void handler::Responder::pack_header(
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

void handler::Responder::send_response_nodata(
		uint8_t status, uint64_t cas)
{
	char* header = (char*)::malloc(MEMPROTO_HEADER_SIZE);
	if(!header) {
		throw std::bad_alloc();
	}

	pack_header(header, status, m_h.opcode,
			0, 0, 0,
			m_h.opaque, cas);
	wavy::request req(&::free, header);
	wavy::write(m_fd, header, MEMPROTO_HEADER_SIZE, req);
}

void handler::Responder::send_response(
		shared_zone& life,
		uint8_t status,
		const char* key, uint16_t keylen,
		const void* val, uint16_t vallen,
		const char* extra, uint16_t extralen,
		uint64_t cas)
{
	char* header = (char*)life->malloc(24);
	pack_header(header, status, m_h.opcode,
			keylen, vallen, extralen,
			m_h.opaque, cas);

	struct iovec vb[4];

	vb[0].iov_base = header;
	vb[0].iov_len  = 24;
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

	wavy::request req(&mp::object_delete<shared_zone>, new shared_zone(life));
	wavy::writev(m_fd, vb, cnt, req);
}


handler::handler(int fd) :
	wavy::handler(fd),
	m_buffer(CLOUDY_INITIAL_ALLOCATION_SIZE),
	m_zone(new msgpack::zone()),
	m_queue(new Queue())
{
	void (*cmd_getx)(void*, memproto_header*,
			const char*, uint16_t) = &mp::object_callback<void (memproto_header*,
				const char*, uint16_t)>
				::mem_fun<handler, &handler::memproto_getx>;

	void (*cmd_set)(void*, memproto_header*,
			const char*, uint16_t,
			const char*, uint32_t,
			uint32_t, uint32_t) = &mp::object_callback<void (memproto_header*,
				const char*, uint16_t,
				const char*, uint32_t,
				uint32_t, uint32_t)>
				::mem_fun<handler, &handler::memproto_set>;

	void (*cmd_delete)(void*, memproto_header*,
			const char*, uint16_t,
			uint32_t) = &mp::object_callback<void (memproto_header*,
				const char*, uint16_t,
				uint32_t)>
				::mem_fun<handler, &handler::memproto_delete>;

	memproto_callback cb = {
		cmd_getx,    // get
		cmd_set,     // set
		NULL,        // add
		NULL,        // replace
		cmd_delete,  // delete
		NULL,        // increment
		NULL,        // decrement
		NULL,        // quit
		NULL,        // flush
		cmd_getx,    // getq
		NULL,        // noop
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
	m_buffer.reserve_buffer(CLOUDY_RESERVE_SIZE);

	size_t rl = ::read(fd(), m_buffer.buffer(), m_buffer.buffer_capacity());
	if(rl < 0) {
		if(errno == EAGAIN || errno == EINTR) {
			return;
		} else {
			throw std::runtime_error("read error");
		}
	} else if(rl == 0) {
		LOG_DEBUG("connection closed: ",strerror(errno));
		throw std::runtime_error("connection closed");
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

		m_zone->push_finalizer(
				&mp::object_delete<mp::stream_buffer::reference>,
				m_buffer.release());

		ret = memproto_dispatch(&m_memproto);
		if(ret <= 0) {
			LOG_DEBUG("unknown command ",(uint16_t)-ret);
			throw std::runtime_error("unknown command");
		}

		m_zone.reset(new msgpack::zone());

	} while(m_buffer.data_size() > 0);

} catch (std::exception& e) {
	LOG_DEBUG("memcached binary protocol error: ",e.what());
	throw;
} catch (...) {
	LOG_DEBUG("memcached binary protocol error: unknown error");
	throw;
}


void handler::memproto_getx(memproto_header* h, const char* key, uint16_t keylen)
{
	LOG_TRACE("getx");

	bool cmd_k = (h->opcode == MEMPROTO_CMD_GETK || h->opcode == MEMPROTO_CMD_GETKQ);
	bool cmd_q = (h->opcode == MEMPROTO_CMD_GETQ || h->opcode == MEMPROTO_CMD_GETKQ);

	ResGet* ctx = m_zone->allocate<ResGet>(h, fd(), m_queue);
	if(cmd_k) { ctx->set_req_key(); }
	if(cmd_q) { ctx->set_req_quiet(); }

	get_request req;
	req.keylen = keylen;
	req.key = key;
	req.hash = gateway::stdhash(req.key, req.keylen);
	req.user = (void*)ctx;
	req.callback = &mp::object_callback<void (get_response&)>
		::mem_fun<ResGet, &ResGet::response>;
	req.life = m_zone;

	gateway::submit(req);
}

void handler::memproto_set(memproto_header* h, const char* key, uint16_t keylen,
		const char* val, uint32_t vallen,
		uint32_t flags, uint32_t expiration)
{
	LOG_TRACE("set");

	if(h->cas || flags || expiration) {
		// FIXME error response
		throw std::runtime_error("memcached binary protocol: invalid argument");
	}

	ResSet* ctx = m_zone->allocate<ResSet>(h, fd(), m_queue);
	set_request req;
	req.keylen = keylen;
	req.key = key;
	req.vallen = vallen;
	req.hash = gateway::stdhash(req.key, req.keylen);
	req.val = val;
	req.user = (void*)ctx;
	req.callback = &mp::object_callback<void (set_response&)>
		::mem_fun<ResSet, &ResSet::response>;
	req.life = m_zone;

	gateway::submit(req);
}

void handler::memproto_delete(memproto_header* h, const char* key, uint16_t keylen,
		uint32_t expiration)
{
	LOG_TRACE("delete");

	if(expiration) {
		// FIXME error response
		throw std::runtime_error("memcached binary protocol: invalid argument");
	}

	ResDelete* ctx = m_zone->allocate<ResDelete>(h, fd(), m_queue);
	delete_request req;
	req.key = key;
	req.keylen = keylen;
	req.hash = gateway::stdhash(req.key, req.keylen);
	req.user = (void*)ctx;
	req.callback = &mp::object_callback<void (delete_response&)>
		::mem_fun<ResDelete, &ResDelete::response>;
	req.life = m_zone;

	gateway::submit(req);
}


static const uint32_t ZERO_FLAG = 0;

void handler::ResGet::response(get_response& res)
{
	if(!is_valid()) { return; }
	LOG_TRACE("get response");

	if(res.error) {
		// error
		if(m_req_quiet) { return; }
		LOG_TRACE("getx res err");
		send_response_nodata(MEMPROTO_RES_INVALID_ARGUMENTS, 0);
		return;
	}

	if(!res.val) {
		// not found
		if(m_req_quiet) { return; }
		send_response_nodata(MEMPROTO_RES_KEY_NOT_FOUND, 0);
		return;
	}

	// found
	send_response(res.life, MEMPROTO_RES_NO_ERROR,
			res.key, (m_req_key ? res.keylen : 0),
			res.val, res.vallen,
			(char*)&ZERO_FLAG, 4,
			0);
}

void handler::ResSet::response(set_response& res)
{
	if(!is_valid()) { return; }
	LOG_TRACE("set response");

	if(res.error) {
		// error
		send_response_nodata(MEMPROTO_RES_OUT_OF_MEMORY, 0);
		return;
	}

	// stored
	send_response_nodata(MEMPROTO_RES_NO_ERROR, 0);
}

void handler::ResDelete::response(delete_response& res)
{
	if(!is_valid()) { return; }
	LOG_TRACE("delete response");

	if(res.error) {
		// error
		send_response_nodata(MEMPROTO_RES_INVALID_ARGUMENTS, 0);
		return;
	}

	if(res.deleted) {
		send_response_nodata(MEMPROTO_RES_NO_ERROR, 0);
	} else {
		send_response_nodata(MEMPROTO_RES_OUT_OF_MEMORY, 0);
	}
}


}  // noname namespace
}  // namespace kumo

