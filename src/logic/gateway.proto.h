#include "gateway/proto.h"
#include "logic/msgtype.h"
#include "logic/client_logic.h"
#include <msgpack.hpp>
#include <string>
#include <stdint.h>

namespace kumo {
namespace gateway {


@message proto_network::HashSpacePush       = 48


@rpc proto_network
	message HashSpacePush.1 {
		msgtype::HSSeed wseed;
		msgtype::HSSeed rseed;
		// acknowledge: true
	};

public:
	void renew_hash_space();
	void renew_hash_space_for(const address& addr);
	RPC_REPLY_DECL(HashSpaceRequest_1, from, res, err, life);
@end


}  // namespace gateway
}  // namespace kumo

