#include "gateway/proto.h"
#include "logic/msgtype.h"
#include "logic/client_logic.h"
#include <msgpack.hpp>
#include <string>
#include <stdint.h>

namespace kumo {
namespace gateway {


@message mod_network_t::HashSpacePush       = 3


@rpc mod_network_t
	message HashSpacePush {
		msgtype::HSSeed wseed;
		msgtype::HSSeed rseed;
		// acknowledge: true
	};

public:
	void keep_alive();

	void renew_hash_space();
	void renew_hash_space_for(const address& addr);
	RPC_REPLY_DECL(HashSpaceRequest, from, res, err, z);
@end


}  // namespace gateway
}  // namespace kumo

