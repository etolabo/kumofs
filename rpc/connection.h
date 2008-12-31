#ifndef RPC_CONNECTION_H__
#define RPC_CONNECTION_H__

#include "log/mlogger.h" //FIXME
#include "rpc/types.h"
#include "rpc/message.h"
#include "rpc/sbuffer.h"
#include "rpc/vrefbuffer.h"
#include "rpc/wavy.h"
#include <msgpack.hpp>
#include <memory>
#include <stdexcept>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifndef RPC_BUFFER_RESERVATION_SIZE
#define RPC_BUFFER_RESERVATION_SIZE (32*1024)
#endif

namespace rpc {


template <typename IMPL>
class connection : public mp::wavy::handler {
public:
	connection(int fd);
	virtual ~connection();

public:
	// from wavy: readable notification
	void read_event();

	void process_message(msgobj msg, msgpack::zone* newz);

	void process_request(method_id method, msgobj param, msgid_t msgid, auto_zone& z)
	{
		throw msgpack::type_error();
	}

	void process_response(msgobj result, msgobj error, msgid_t msgid, auto_zone& z)
	{
		throw msgpack::type_error();
	}

private:
	msgpack::unpacker m_pac;

private:
	connection();
	connection(const connection&);
};


template <typename IMPL>
connection<IMPL>::connection(int fd) :
	mp::wavy::handler(fd) { }

template <typename IMPL>
connection<IMPL>::~connection() { }


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
		wavy::submit(&IMPL::process_message,
				shared_self<IMPL>(), msg, z.get());
		z.release();
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
void connection<IMPL>::process_message(msgobj msg, msgpack::zone* newz)
try {
	auto_zone z(z);
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


}  // namespace rpc

#endif /* rpc/connection.h */

