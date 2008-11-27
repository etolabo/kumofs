#ifndef RPC_PROTOCOL_H__
#define RPC_PROTOCOL_H__

#include <msgpack.hpp>

namespace rpc {

namespace protocol {
	using msgpack::define;
	using msgpack::type::tuple;
	using msgpack::type::raw_ref;

	static const int TRANSPORT_LOST_ERROR = 1;
	static const int NODE_LOST_ERROR      = 2;
	static const int TIMEOUT_ERROR        = 3;
	static const int UNKNOWN_ERROR        = 4;
	static const int PROTOCOL_ERROR       = 5;
	static const int SERVER_ERROR         = 6;
}

}  // namespace rpc

#endif /* rpc/protocol.h */

