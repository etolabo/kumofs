#ifndef RPC_MESSAGE_H__
#define RPC_MESSAGE_H__

#include <msgpack.hpp>

namespace rpc {


template <uint16_t Protocol, uint16_t Version, typename Session>
struct message {
	typedef Session session_type;
	typedef rpc::method<Protocol, Version> method;
};


}  // namespace rpc

#endif /* rpc/message.h */

