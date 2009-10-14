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

