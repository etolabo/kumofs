#include "gateway/memproto.h"
#include "memproto/memproto.h"
#include <stdexcept>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <algorithm>
#include <memory>
#include <deque>

namespace kumo {


static const size_t MEMPROTO_INITIAL_ALLOCATION_SIZE = 2048;
static const size_t MEMPROTO_RESERVE_SIZE = 1024;


Memproto::Memproto(int lsock) :
	m_lsock(lsock) { }

Memproto::~Memproto() {}


void Memproto::accepted(void* data, int fd)
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

void Memproto::listen(Gateway* gw)
{
	mp::iothreads::listen(m_lsock,
			&Memproto::accepted,
			reinterpret_cast<void*>(gw));
}



class Memproto::Connection : public iothreads::handler {
public:
	Connection(int fd, Gateway* gw);
	~Connection();

public:
	void read_event();

private:
	// get, getq, getk, getkq
	inline void memproto_getx(memproto_header* h, const char* key, uint16_t keylen);

	// set
	inline void memproto_set(memproto_header* h, const char* key, uint16_t keylen,
			const char* val, uint16_t vallen,
			uint32_t flags, uint32_t expiration);

	// delete
	inline void memproto_delete(memproto_header* h, const char* key, uint16_t keylen,
			uint32_t expiration);

	// noop
	inline void memproto_noop(memproto_header* h);

private:
	memproto_text m_memproto;
	char* m_buffer;
	size_t m_free;
	size_t m_used;
	size_t m_off;
	Gateway* m_gw;

	class Responder;

	class Queue {
	public:
		bool is_valid() const { return m_valid; }
		void process_queue();
	private:
		int m_fd;
		typedef std::deque<Responder*> queue_t;
		queue_t m_queue;
		bool m_valid;
	};
	typedef mp::shared_ptr<Queue> SharedQueue;
	SharedQueue m_valid;

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
		Responder(int fd, SharedQueue& valid) :
			m_fd(fd), m_valid(valid) { }
		~Responder() { }

		bool is_valid() const { return *m_valid; }

		int fd() const { return m_fd; }

		void send_data(const char* buf, size_t buflen);
		void send_datav(struct iovec* vb, size_t count, shared_zone& life);

	private:
		int m_fd;
		SharedQueue m_valid;
	};

	struct ResGet : Responder {
		ResGet(int fd, SharedQueue& valid) :
			Responder(fd, valid) { }
		~ResGet() { }
		void response(get_response& res);
		void response_q(get_response& res);
		void response_k(get_response& res);
		void response_kq(get_response& res);
	};


	template <typename T>
	char* alloc_responder_buf(shared_zone& z, size_t size, T** rp);

	template <typename T>
	static void object_destruct_free(void* data);

private:
	Connection();
	Connection(const Connection&);
};




class Memproto::Connection : public iothreads::handler {
public:
	Connection(int fd, Gateway* gw);
	~Connection();

public:
	void read_event();

private:
	// get, getq, getk, getkq
	inline void memproto_getx(memproto_header* h, const char* key, uint16_t keylen);

	// set
	inline void memproto_set(memproto_header* h, const char* key, uint16_t keylen,
			const char* val, uint16_t vallen,
			uint32_t flags, uint32_t expiration);

	// delete
	inline void memproto_delete(memproto_header* h, const char* key, uint16_t keylen,
			uint32_t expiration);

	// noop
	inline void memproto_noop(memproto_header* h);

private:
	memproto_parser m_memproto;
	char* m_buffer;
	size_t m_free;
	size_t m_used;
	size_t m_off;
	shared_zone m_zone;
	Gateway* m_gw;

	mp::shared_ptr<SharedResponder> m_responder;

public:
	struct Request {
		Request(shared_zone life_) : done(false), life(life_) {}
		bool done;
		struct iovec vec[4];  // NOTE: check send_response()
		unsigned short veclen;
		shared_zone life;
	private:
		Request();
		Request(const Request&);
	};

private:
	Connection();
	Connection(const Connection&);
};


class Memproto::SharedResponder {
public:
	SharedResponder(int fd);
	~SharedResponder();

public:
	bool is_valid() const;
	void invalidate();

public:
	typedef Connection::Request Request;
	typedef Gateway::user_get_response user_get_response;
	typedef Gateway::user_set_response user_set_response;
	typedef Gateway::user_delete_response user_delete_response;

	void memproto_getx_response(
			Request* req, memproto_header h,
			const char* key, uint32_t keylen,
			user_get_response ret, shared_zone life);

	void memproto_set_response(
			Request* req, memproto_header h,
			user_set_response ret, shared_zone life);

	void memproto_delete_response(
			Request* req, memproto_header h,
			user_delete_response ret, shared_zone life);

private:
	void send_response_quiet(Request* req);

	void send_response_nodata(
			Request* req, memproto_header* h,
			uint8_t status, uint64_t cas);

	void send_response(
			Request* req, memproto_header* h,
			uint8_t status,
			const char* key, uint16_t keylen,
			const void* val, uint16_t vallen,
			const char* extra, uint16_t extralen,
			uint64_t cas);

	static void pack_header(char* hbuf, uint16_t status, uint8_t op,
			uint16_t keylen, uint32_t vallen, uint8_t extralen,
			uint32_t opaque, uint64_t cas);

private:
	void process_queue();

private:
	int m_fd;

	typedef std::deque<Request*> queue_t;
	queue_t m_queue;
	friend class Connection;

private:
	SharedResponder();
	SharedResponder(const SharedResponder&);
};


#define BIND_RESPONSE(gw_cmd, res_cmd, ...) \
	Gateway::user ##gw_cmd ##_callback_t( \
			mp::bind(&SharedResponder::memproto ##res_cmd ##_response, \
				m_responder, __VA_ARGS__))


Memproto::Connection::Connection(int fd, Gateway* gw) :
	mp::iothreads::handler(fd),
	m_buffer(NULL),
	m_free(0),
	m_used(0),
	m_off(0),
	m_zone(new mp::zone()),
	m_gw(gw),
	m_responder(new SharedResponder(fd))
{
	void (*cmd_getx)(void*, memproto_header*,
			const char*, uint16_t) = &mp::object_callback<void (memproto_header*,
				const char*, uint16_t)>
				::mem_fun<Connection, &Connection::memproto_getx>;

	void (*cmd_set)(void*, memproto_header*,
			const char*, uint16_t,
			const char*, uint16_t,
			uint32_t, uint32_t) = &mp::object_callback<void (memproto_header*,
				const char*, uint16_t,
				const char*, uint16_t,
				uint32_t, uint32_t)>
				::mem_fun<Connection, &Connection::memproto_set>;

	void (*cmd_delete)(void*, memproto_header*,
			const char*, uint16_t,
			uint32_t) = &mp::object_callback<void (memproto_header*,
				const char*, uint16_t,
				uint32_t)>
				::mem_fun<Connection, &Connection::memproto_delete>;

	void (*cmd_noop)(void*, memproto_header*) =
			&mp::object_callback<void (memproto_header*)>
				::mem_fun<Connection, &Connection::memproto_noop>;

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
		cmd_noop,    // noop
		NULL,        // version
		cmd_getx,    // getk
		cmd_getx,    // getkq
		NULL,        // append
		NULL,        // prepend
	};

	memproto_parser_init(&m_memproto, &cb, this);
}

Memproto::Connection::~Connection()
{
	m_responder->invalidate();
	::free(m_buffer);
}


inline Memproto::SharedResponder::SharedResponder(int fd) :
	m_fd(fd)
{ }

inline Memproto::SharedResponder::~SharedResponder()
{ }

inline bool Memproto::SharedResponder::is_valid() const
{
	return m_fd >= 0;
}

inline void Memproto::SharedResponder::invalidate()
{
	m_fd = -1;
}


namespace {
static void finalize_free(void* buf) { ::free(buf); }
}  // noname namespace

void Memproto::Connection::read_event()
try {
	if(m_free < MEMPROTO_RESERVE_SIZE) {
		size_t nsize;
		if(m_buffer == NULL) { nsize = MEMPROTO_INITIAL_ALLOCATION_SIZE; }
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

	int ret;
	while( (ret = memproto_parser_execute(&m_memproto, m_buffer, m_used, &m_off)) > 0) {
		m_zone->push_finalizer(finalize_free, m_buffer);
		size_t trail = m_used - m_off;
		if(trail > 0) {
			char* curbuf = m_buffer;
			m_buffer = NULL;  // prevent double-free at destructor
			size_t nsize = MEMPROTO_INITIAL_ALLOCATION_SIZE;
			while(nsize < trail) { nsize *= 2; }
			char* nbuffer = (char*)::malloc(nsize);
			if(!nbuffer) { throw std::bad_alloc(); }
			memcpy(nbuffer, curbuf+m_off, trail);
			m_buffer = nbuffer;
			m_free = nsize - trail;
			m_used = trail;
		} else {
			m_buffer = NULL;
			m_free = 0;
			m_used = 0;
		}
		m_off = 0;
		if( (ret = memproto_dispatch(&m_memproto)) <= 0) {
			LOG_WARN("unknown command ",(-ret));
			throw std::runtime_error("unknown command");
		}
		m_zone.reset(new mp::zone());
	}

	if(ret < 0) { throw std::runtime_error("parse error"); }

} catch (std::runtime_error& e) {
	LOG_DEBUG("memcached binary protocol error: ",e.what());
	throw;
} catch (...) {
	LOG_DEBUG("memcached binary protocol error: unknown error");
	throw;
}


void Memproto::Connection::memproto_getx(memproto_header* h, const char* key, uint16_t keylen)
{
	LOG_TRACE("getx");

	std::auto_ptr<Request> req(new Request(m_zone));
	m_responder->m_queue.push_back(req.get());

	using namespace mp::placeholders;
	mp::iothreads::submit(&Gateway::user_get, m_gw,
			key, keylen, m_zone,
			BIND_RESPONSE(_get, _getx, req.get(), *h,
				key, keylen, _1, _2));

	req.release();
}

namespace {
	static const uint32_t ZERO_FLAG = 0;
}  // noname namespace

void Memproto::SharedResponder::memproto_getx_response(
		Request* req, memproto_header h,
		const char* key, uint32_t keylen,
		user_get_response ret, shared_zone life)
{
	if(!is_valid()) { return; }
	LOG_TRACE("getx callbacked ",(uint16_t)h.opcode);

	bool cmd_k = (h.opcode == MEMPROTO_CMD_GETK || h.opcode == MEMPROTO_CMD_GETKQ);
	bool cmd_q = (h.opcode == MEMPROTO_CMD_GETQ || h.opcode == MEMPROTO_CMD_GETKQ);

	if(!ret.success) {
		if(cmd_q) {
			send_response_quiet(req);
		} else {
			LOG_TRACE("getx res err");
			send_response_nodata(req, &h,
					MEMPROTO_RES_INVALID_ARGUMENTS, 0);
		}
	} else if(ret.val) {
		LOG_TRACE("getx res found");
		send_response(req, &h,
				MEMPROTO_RES_NO_ERROR,
				key, (cmd_k ? keylen : 0),
				ret.val, ret.vallen,
				(char*)&ZERO_FLAG, 4,
				0);
	} else {
		if(cmd_q) {
			send_response_quiet(req);
		} else {
			LOG_TRACE("getx res not found");
			send_response_nodata(req, &h,
					MEMPROTO_RES_KEY_NOT_FOUND, 0);
		}
	}

	LOG_TRACE("getx responsed");
}


void Memproto::Connection::memproto_set(memproto_header* h, const char* key, uint16_t keylen,
		const char* val, uint16_t vallen,
		uint32_t flags, uint32_t expiration)
{
	LOG_TRACE("set");

	if(h->cas || flags || expiration) {
		throw std::runtime_error("memcached binary protocol: invalid argument");
	}

	std::auto_ptr<Request> req(new Request(m_zone));
	m_responder->m_queue.push_back(req.get());

	using namespace mp::placeholders;
	mp::iothreads::submit(&Gateway::user_set, m_gw,
			key, keylen, val, vallen, m_zone,
			BIND_RESPONSE(_set, _set, req.get(), *h,
				_1, _2));

	req.release();
}

void Memproto::SharedResponder::memproto_set_response(
		Request* req, memproto_header h,
		user_set_response ret, shared_zone life)
{
	if(!is_valid()) { return; }
	LOG_TRACE("set callbacked");

	if(!ret.success) {
		LOG_ERROR("set error");
		send_response_nodata(req, &h, MEMPROTO_RES_OUT_OF_MEMORY, 0);
	} else {
		send_response_nodata(req, &h, MEMPROTO_RES_NO_ERROR, 0);
	}
}


void Memproto::Connection::memproto_delete(memproto_header* h, const char* key, uint16_t keylen,
		uint32_t expiration)
{
	LOG_TRACE("delete");

	if(expiration) {
		throw std::runtime_error("memcached binary protocol: invalid argument");
	}

	std::auto_ptr<Request> req(new Request(m_zone));
	m_responder->m_queue.push_back(req.get());

	using namespace mp::placeholders;
	mp::iothreads::submit(&Gateway::user_delete, m_gw,
			key, keylen, m_zone,
			BIND_RESPONSE(_delete, _delete, req.get(), *h,
				_1, _2));

	req.release();
}


void Memproto::SharedResponder::memproto_delete_response(
		Request* req, memproto_header h,
		user_delete_response ret, shared_zone life)
{
	if(!is_valid()) { return; }
	LOG_TRACE("delete callbacked");

	if(!ret.success) {
		send_response_nodata(req, &h, MEMPROTO_RES_INVALID_ARGUMENTS, 0);
	} else if(ret.deleted) {
		send_response_nodata(req, &h, MEMPROTO_RES_NO_ERROR, 0);
	} else {
		send_response_nodata(req, &h, MEMPROTO_RES_OUT_OF_MEMORY, 0);
	}
}


void Memproto::Connection::memproto_noop(memproto_header* h)
{
	LOG_TRACE("noop");

	std::auto_ptr<Request> req(new Request(m_zone));
	m_responder->m_queue.push_back(req.get());

	mp::iothreads::submit(&SharedResponder::send_response_nodata, m_responder,
			req.get(), h,
			MEMPROTO_RES_NO_ERROR, 0);

	req.release();
}


void Memproto::SharedResponder::pack_header(char* hbuf, uint16_t status, uint8_t op,
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

inline void Memproto::SharedResponder::send_response_quiet(Request* req)
{
	req->veclen = 0;
	req->done = true;
	process_queue();
}

void Memproto::SharedResponder::send_response_nodata(
		Request* req, memproto_header* h,
		uint8_t status, uint64_t cas)
{
	char* header = (char*)req->life->malloc(24);
	pack_header(header, status, h->opcode,
			0, 0, 0,
			h->opaque, cas);
	req->vec[0].iov_base = header;
	req->vec[0].iov_len  = 24;
	req->veclen = 1;
	req->done = true;
	process_queue();
}


namespace {
struct shared_zone_keeper {
	shared_zone_keeper(shared_zone& z) : m(z) {}
	shared_zone m;
};
}  // noname namespace

inline void Memproto::SharedResponder::send_response(
		Request* req, memproto_header* h,
		uint8_t status,
		const char* key, uint16_t keylen,
		const void* val, uint16_t vallen,
		const char* extra, uint16_t extralen,
		uint64_t cas)
{
	char* header = (char*)req->life->malloc(24);
	pack_header(header, status, h->opcode,
			keylen, vallen, extralen,
			h->opaque, cas);

	req->vec[0].iov_base = header;
	req->vec[0].iov_len  = 24;
	req->veclen = 1;

	if(extralen > 0) {
		req->vec[req->veclen].iov_base = const_cast<char*>(extra);
		req->vec[req->veclen].iov_len  = extralen;
		++req->veclen;
	}

	if(keylen > 0) {
		req->vec[req->veclen].iov_base = const_cast<char*>(key);
		req->vec[req->veclen].iov_len  = keylen;
		++req->veclen;
	}

	if(vallen > 0) {
		req->vec[req->veclen].iov_base = const_cast<void*>(val);
		req->vec[req->veclen].iov_len  = vallen;
		++req->veclen;
	}

	req->done = true;
	process_queue();
}


void Memproto::SharedResponder::process_queue()
{
	while(!m_queue.empty()) {
		Request* req(m_queue.front());
		if(!req->done) { break; }
		if(req->veclen > 0) {
			mp::iothreads::send_datav(m_fd, req->vec, req->veclen, *req->life, true);
		}
		m_queue.pop_front();
		delete req;
	}
}


}  // namespace kumo

