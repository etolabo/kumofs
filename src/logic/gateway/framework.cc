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
#include "gateway/framework.h"

namespace kumo {
namespace gateway {


std::auto_ptr<framework> net;
std::auto_ptr<resource> share;


void framework::dispatch(
		shared_session from, weak_responder response,
		rpc::method_id method, rpc::msgobj param, auto_zone z)
try {
	switch(method.get()) {
	RPC_DISPATCH(mod_network, HashSpacePush);
	default:
		throw unknown_method_error();
	}
}
DISPATCH_CATCH(method, response)


void framework::session_lost(const address& addr, shared_session& s)
{
	LOG_WARN("lost session ",addr);
	if(addr == share->manager1() || addr == share->manager2()) {
		mod_network.renew_hash_space_for(addr);
	}
}


}  // namespace gateway
}  // namespace kumo

