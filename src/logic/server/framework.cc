#include "server/framework.h"

namespace kumo {
namespace server {


std::auto_ptr<framework> net;
std::auto_ptr<resource> share;


void framework::cluster_dispatch(
		shared_node from, weak_responder response,
		rpc::method_id method, rpc::msgobj param, auto_zone z)
{
	// FIXME try & catch
	switch(method.get()) {
	RPC_DISPATCH(proto_network, KeepAlive_1);
	RPC_DISPATCH(proto_network, HashSpaceSync_1);
	RPC_DISPATCH(proto_store,   ReplicateSet_1);
	RPC_DISPATCH(proto_store,   ReplicateDelete_1);
	RPC_DISPATCH(proto_replace, ReplaceCopyStart_1);
	RPC_DISPATCH(proto_replace, ReplaceDeleteStart_1);
	RPC_DISPATCH(proto_replace_stream, ReplaceOffer_1);
	RPC_DISPATCH(proto_control, CreateBackup_1);
	default:
		// FIXME exception class
		throw std::runtime_error("unknown method");
	}
}

void framework::subsystem_dispatch(
		shared_peer from, weak_responder response,
		rpc::method_id method, rpc::msgobj param, auto_zone z)
{
	// FIXME try & catch
	switch(method.get()) {
	RPC_DISPATCH(proto_store,   Get_1);
	RPC_DISPATCH(proto_store,   Set_1);
	RPC_DISPATCH(proto_store,   Delete_1);
	RPC_DISPATCH(proto_control, GetStatus_1);
	default:
		// FIXME exception class
		throw std::runtime_error("unknown method");
	}
}

void framework::run()
{
	wavy_server::run();
	scope_proto_replace_stream().run_stream();
	// FIXME end
}

void framework::end_preprocess()
{
	scope_proto_replace_stream().stop_stream();
}


void framework::step_timeout()
{
	rpc::cluster::step_timeout();
}


void framework::new_node(address addr, role_type id, shared_node n)
{
	// XXX
	LOG_WARN("new node ",(uint16_t)id," ",addr);
	if(addr == share->manager1()) {
		scope_proto_network().renew_r_hash_space();
		scope_proto_network().renew_w_hash_space();
	} else if(share->manager2().connectable() && addr == share->manager2()) {
		scope_proto_network().renew_r_hash_space();
		scope_proto_network().renew_w_hash_space();
	}
}

void framework::lost_node(address addr, role_type id)
{
	// XXX
	LOG_WARN("lost node ",(uint16_t)id," ",addr);
}


}  // namespace server
}  // namespace kumo

