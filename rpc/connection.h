#ifndef RPC_CONNECTION_H__
#define RPC_CONNECTION_H__

#include "log/mlogger.h" //FIXME
#include "rpc/message.h"
#include "rpc/sbuffer.h"
#include "rpc/vrefbuffer.h"
#include <mp/iothreads.h>
#include <msgpack.hpp>
#include <memory>
#include <stdexcept>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifndef RPC_BUFFER_RESERVATION_SIZE
#define RPC_BUFFER_RESERVATION_SIZE 1024
#endif

namespace rpc {


typedef msgpack::object msgobj;


class responder;

template <typename IMPL>
class connection : public mp::iothreads::handler {
public:
	connection(int fd);
	virtual ~connection();

	typedef std::auto_ptr<msgpack::zone> auto_zone;
	typedef rpc::responder   responder;

public:
	// from iothreads: readable notification
	void read_event();

	void process_message(msgobj msg, auto_zone&);

	void process_request(method_id method, msgobj param, msgid_t msgid, auto_zone& z);

	void dispatch_request(method_id method, msgobj param, responder& response, auto_zone& z)
	{
		throw msgpack::type_error();
	}

	void process_response(msgobj result, msgobj error, msgid_t msgid, auto_zone& z)
	{
		throw msgpack::type_error();
	}

private:
	msgpack::unpacker m_pac;
	//mp::shared_count m_count;  FIXME

private:
	connection();
	connection(const connection&);
};

class responder {
public:
	responder(int fd, msgid_t msgid);

	template <typename IMPL>
	responder(connection<IMPL>* con, msgid_t msgid);

	~responder();

	template <typename Result>
	void result(Result res);

	template <typename Result>
	void result(Result res, mp::zone& life);

	template <typename Error>
	void error(Error err);

	template <typename Error>
	void error(Error err, mp::zone& life);

	void null();

	void send_response(const void* buf, size_t buflen,
			mp::zone& z, bool thread_safe = true);

	void send_responsev(const struct iovec* vb, size_t count,
			mp::zone& z, bool thread_safe = true);

private:
	template <typename Result, typename Error>
	void call(Result& res, Error& err);

	template <typename Result, typename Error>
	void call(Result& res, Error& err, mp::zone& life);

private:
	int m_fd;
	msgid_t m_msgid;
	//mp::weak_count m_count;  FIXME

private:
	responder();
};


template <typename IMPL>
connection<IMPL>::connection(int fd) :
	mp::iothreads::handler(fd) { }

template <typename IMPL>
connection<IMPL>::~connection() { }


template <typename IMPL>
void connection<IMPL>::process_request(method_id method, msgobj param, msgid_t msgid, auto_zone& z)
{
	responder response(fd(), msgid);
	static_cast<IMPL*>(this)->dispatch_request(
			method, param, response, z);
}


template <typename IMPL>
void connection<IMPL>::connection::read_event()
try {
	m_pac.reserve_buffer(RPC_BUFFER_RESERVATION_SIZE);

	ssize_t rl = ::read(fd(), m_pac.buffer(), m_pac.buffer_capacity());
	if(rl < 0) {
		if(errno == EAGAIN || errno == EINTR) {
			return;
		} else {
			throw std::runtime_error("read error");
		}
	} else if(rl == 0) {
		throw std::runtime_error("connection closed");
	}

	m_pac.buffer_consumed(rl);

	while(m_pac.execute()) {
		msgobj msg = m_pac.data();
		std::auto_ptr<msgpack::zone> z( m_pac.release_zone() );
		m_pac.reset();
		static_cast<IMPL*>(this)->process_message(msg, z);
	}

} catch(msgpack::type_error& e) {
	LOG_ERROR("rpc packet: type error");
	throw;
} catch(std::exception& e) {
	LOG_WARN("rpc packet: ", e.what());
	throw;
} catch(...) {
	LOG_ERROR("rpc packet: unknown error");
	throw;
}


template <typename IMPL>
void connection<IMPL>::process_message(msgobj msg, auto_zone& z)
{
	rpc_message rpc(msg.convert());
	if(rpc.is_request()) {
		rpc_request<msgobj> msgreq(rpc);
		static_cast<IMPL*>(this)->process_request(
				msgreq.method(), msgreq.param(), msgreq.msgid(), z);
	} else {
		rpc_response<msgobj, msgobj> msgres(rpc);
		static_cast<IMPL*>(this)->process_response(
				msgres.result(), msgres.error(), msgres.msgid(), z);
	}
}

inline responder::responder(int fd, msgid_t msgid) :
	m_fd(fd), m_msgid(msgid) { }

template <typename IMPL>
responder::responder(connection<IMPL>* con, msgid_t msgid):
	m_fd(con->fd()), m_msgid(msgid) { }

inline responder::~responder() { }

template <typename Result>
void responder::result(Result res)
{
	msgpack::type::nil err;
	call(res, err);
}

template <typename Result>
void responder::result(Result res, mp::zone& life)
{
	msgpack::type::nil err;
	call(res, err, life);
}

template <typename Error>
void responder::error(Error err)
{
	msgpack::type::nil res;
	call(res, err);
}

template <typename Error>
void responder::error(Error err, mp::zone& life)
{
	msgpack::type::nil res;
	call(res, err, life);
}

inline void responder::null()
{
	msgpack::type::nil res;
	msgpack::type::nil err;
	call(res, err);
}

template <typename Result, typename Error>
inline void responder::call(Result& res, Error& err)
{
	rpc::sbuffer buf;  // FIXME use vrefbuffer?
	rpc_response<Result&, Error> msgres(res, err, m_msgid);
	msgpack::pack(buf, msgres);
	mp::iothreads::send_data(m_fd, buf.data(), buf.size(),
			&mp::iothreads::writer::finalize_free, buf.data());
	buf.release();
}

template <typename Result, typename Error>
inline void responder::call(Result& res, Error& err, mp::zone& life)
{
	vrefbuffer* buf = life.allocate<vrefbuffer>();
	rpc_response<Result&, Error> msgres(res, err, m_msgid);
	msgpack::pack(*buf, msgres);

	size_t sz = buf->vector_size();
	struct iovec vb[sz];
	buf->get_vector(vb);
	mp::iothreads::send_datav(m_fd, vb, sz, life, true);
}

//inline void responder::send_response(const void* buf, size_t buflen,
//		mp::zone& z, bool thread_safe)
//{
	// FIXME
//	//mp::iothreads::send_data(m_fd, buf, buflen, z, thread_safe);
//}

inline void responder::send_responsev(const struct iovec* vb, size_t count,
		mp::zone& z, bool thread_safe)
{
	mp::iothreads::send_datav(m_fd, vb, count, z, thread_safe);
}


}  // namespace rpc

#endif /* rpc/connection.h */

