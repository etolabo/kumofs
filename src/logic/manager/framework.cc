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
#include "manager/framework.h"

namespace kumo {
namespace manager {


std::auto_ptr<framework> net;
std::auto_ptr<resource> share;


void framework::cluster_dispatch(
		shared_node from, weak_responder response,
		rpc::method_id method, rpc::msgobj param, auto_zone z)
try {
	// FIXME try & catch
	switch(method.get()) {
	RPC_DISPATCH(mod_network, KeepAlive);
	RPC_DISPATCH(mod_network, WHashSpaceRequest);
	RPC_DISPATCH(mod_network, RHashSpaceRequest);
	RPC_DISPATCH(mod_network, HashSpaceSync);
	RPC_DISPATCH(mod_replace, ReplaceCopyEnd);
	RPC_DISPATCH(mod_replace, ReplaceDeleteEnd);
	RPC_DISPATCH(mod_replace, ReplaceElection);
	default:
		throw unknown_method_error();
	}
}
DISPATCH_CATCH(method, response)

void framework::subsystem_dispatch(
		shared_peer from, weak_responder response,
		rpc::method_id method, rpc::msgobj param, auto_zone z)
try {
	// FIXME try & catch
	switch(method.get()) {
	RPC_DISPATCH(mod_network, HashSpaceRequest);
	RPC_DISPATCH(mod_control, GetNodesInfo);
	RPC_DISPATCH(mod_control, AttachNewServers);
	RPC_DISPATCH(mod_control, DetachFaultServers);
	RPC_DISPATCH(mod_control, CreateBackup);
	RPC_DISPATCH(mod_control, SetAutoReplace);
	RPC_DISPATCH(mod_control, StartReplace);
	default:
		throw unknown_method_error();
	}
}
DISPATCH_CATCH(method, response)


void framework::new_node(address addr, role_type id, shared_node n)
{
	LOG_WARN("new node ",(uint16_t)id," ",addr);
	if(id == ROLE_MANAGER) {
		if(addr != share->partner()) {
			TLOGPACK("eP",3,
					"addr",addr);
			LOG_ERROR("unknown partner node");
			// FIXME
			return;
		}
		LOG_INFO("partner connected ",addr);
		{
			pthread_scoped_lock hslk(share->hs_mutex());
			mod_network.sync_hash_space_partner(hslk);
		}
		return;

	} else if(id == ROLE_SERVER) {
		// FIXME delayed change
		mod_replace.add_server(addr, n);
		return;

	} else {
		LOG_ERROR("unkown node id ",(uint16_t)id);
	}
}

void framework::lost_node(address addr, role_type id)
{
	LOG_WARN("lost node ",(uint16_t)id," ",addr);
	if(id == ROLE_MANAGER) {
		return;

	} else if(id == ROLE_SERVER) {
		// FIXME delayed change
		mod_replace.remove_server(addr);
		return;
	}
}


}  // namespace manager
}  // namespace kumo

