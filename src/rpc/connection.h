//
// kumofs
//
// Copyright (C) 2009 Etolabo Corp.
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
#ifndef RPC_CONNECTION_H__
#define RPC_CONNECTION_H__

#include "log/mlogger.h" //FIXME
#include "rpc/types.h"
#include "rpc/protocol.h"
#include "rpc/wavy.h"
#include "rpc/exception.h"
#include <msgpack.hpp>
#include <stdexcept>
#include <memory>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifndef RPC_INITIAL_BUFFER_SIZE
#define RPC_INITIAL_BUFFER_SIZE (64*1024)
#endif

#ifndef RPC_BUFFER_RESERVATION_SIZE
#define RPC_BUFFER_RESERVATION_SIZE (8*1024)
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

	void submit_message(msgobj msg, auto_zone& z);

	void process_message(msgobj msg, msgpack::zone* newz);

	void process_request(method_id method, msgobj param, msgid_t msgid, auto_zone& z);

	void dispatch_request(method_id method, msgobj param, responder& response, auto_zone& z);

	void process_response(msgobj result, msgobj error, msgid_t msgid, auto_zone& z);

private:
	msgpack::unpacker m_pac;

private:
	connection();
	connection(const connection&);
};


template <typename IMPL>
connection<IMPL>::connection(int fd) :
	mp::wavy::handler(fd),
	m_pac(RPC_INITIAL_BUFFER_SIZE) { }

template <typename IMPL>
connection<IMPL>::~connection() { }


template <typename IMPL>
void connection<IMPL>::connection::read_event()
try {
	m_pac.reserve_buffer(RPC_BUFFER_RESERVATION_SIZE);

	ssize_t rl = ::read(fd(), m_pac.buffer(), m_pac.buffer_capacity());
	if(rl <= 0) {
		if(rl == 0) { throw connection_closed_error(); }
		if(errno == EAGAIN || errno == EINTR) { return; }
		else { throw connection_broken_error(); }
	}

	m_pac.buffer_consumed(rl);

	while(m_pac.execute()) {
		msgobj msg = m_pac.data();
		std::auto_ptr<msgpack::zone> z( m_pac.release_zone() );
		m_pac.reset();
		static_cast<IMPL*>(this)->submit_message(msg, z);
	}

} catch(connection_error& e) {
	LOG_DEBUG(e.what());
	throw;
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
inline void connection<IMPL>::submit_message(msgobj msg, auto_zone& z)
{
	// FIXME better performance?
	static_cast<IMPL*>(this)->process_message(msg, z.release());
	//wavy::submit(&IMPL::process_message,
	//		shared_self<IMPL>(), msg, z.get());
	//z.release();
}


template <typename IMPL>
inline void connection<IMPL>::dispatch_request(method_id method, msgobj param,
		responder& response, auto_zone& z)
{
	throw msgpack::type_error();
}

template <typename IMPL>
inline void connection<IMPL>::process_response(msgobj result, msgobj error,
		msgid_t msgid, auto_zone& z)
{
	throw msgpack::type_error();
}

template <typename IMPL>
inline void connection<IMPL>::process_request(method_id method, msgobj param,
		msgid_t msgid, auto_zone& z)
{
	responder response(fd(), msgid);
	static_cast<IMPL*>(this)->dispatch_request(
			method, param, response, z);
}


template <typename IMPL>
void connection<IMPL>::process_message(msgobj msg, msgpack::zone* newz)
try {
	auto_zone z(newz);
	rpc_message rpc(msg.convert());

	if(rpc.is_request()) {
		rpc_request<msgobj> msgreq(msg.convert());
		static_cast<IMPL*>(this)->process_request(
				msgreq.method(), msgreq.param(), msgreq.msgid(), z);

	} else {
		rpc_response<msgobj, msgobj> msgres(msg.convert());
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

