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
	RPC_DISPATCH(proto_network, KeepAlive);
	RPC_DISPATCH(proto_network, WHashSpaceRequest);
	RPC_DISPATCH(proto_network, RHashSpaceRequest);
	RPC_DISPATCH(proto_network, HashSpaceSync);
	RPC_DISPATCH(proto_replace, ReplaceCopyEnd);
	RPC_DISPATCH(proto_replace, ReplaceDeleteEnd);
	RPC_DISPATCH(proto_replace, ReplaceElection);
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
	RPC_DISPATCH(proto_network, HashSpaceRequest);
	RPC_DISPATCH(proto_control, GetNodesInfo);
	RPC_DISPATCH(proto_control, AttachNewServers);
	RPC_DISPATCH(proto_control, DetachFaultServers);
	RPC_DISPATCH(proto_control, CreateBackup);
	RPC_DISPATCH(proto_control, SetAutoReplace);
	RPC_DISPATCH(proto_control, StartReplace);
	default:
		throw unknown_method_error();
	}
}
DISPATCH_CATCH(method, response)


void framework::new_node(address addr, role_type id, shared_node n)
{
	LOG_WARN("new node ",id," ",addr);
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
			scope_proto_network().sync_hash_space_partner(hslk);
		}
		return;

	} else if(id == ROLE_SERVER) {
		// FIXME delayed change
		scope_proto_replace().add_server(addr, n);
		return;

	} else {
		LOG_ERROR("unkown node id ",(uint16_t)id);
	}
}

void framework::lost_node(address addr, role_type id)
{
	LOG_WARN("lost node ",id," ",addr);
	if(id == ROLE_MANAGER) {
		return;

	} else if(id == ROLE_SERVER) {
		// FIXME delayed change
		scope_proto_replace().remove_server(addr);
		return;

	}
}


}  // namespace manager
}  // namespace kumo

