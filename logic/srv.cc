#include "logic/srv_impl.h"

namespace kumo {


Server::~Server()
{
}

Server::CliSrv::CliSrv(Server* srv) :
	CliSrvBase<Server>(srv) { }

void Server::cluster_dispatch(
		shared_node& from, role_type role, rpc::weak_responder response,
		method_id method, msgobj param, shared_zone& life)
{
	if(role == protocol::MANAGER) {
		switch(method) {
		RPC_DISPATCH(KeepAlive);
		RPC_DISPATCH(HashSpacePush);
		RPC_DISPATCH(ReplaceCopyStart);
		RPC_DISPATCH(ReplaceDeleteStart);
		RPC_DISPATCH(CreateBackup);
		default:
			throw std::runtime_error("unknown method");
		}

	} else if(role == protocol::SERVER) {
		switch(method) {
		RPC_DISPATCH(ReplicateSet);
		RPC_DISPATCH(ReplicateDelete);
		RPC_DISPATCH(ReplacePush);
		default:
			throw std::runtime_error("unknown method");
		}

	} else {
		throw std::runtime_error("unknown role");
	}
}


void Server::CliSrv::dispatch(
		shared_peer& from, rpc::weak_responder response,
		method_id method, msgobj param, shared_zone& life)
{
	switch(method) {
	CLISRV_DISPATCH(Get);
	CLISRV_DISPATCH(Set);
	CLISRV_DISPATCH(Delete);
	default:
		throw std::runtime_error("unknown method");
	}
}


void Server::step_timeout()
{
	rpc::cluster::step_timeout();
	m_clisrv.step_timeout();
}


void Server::new_node(address addr, role_type id, shared_node n)
{
	// XXX
	LOG_WARN("new node ",id," ",addr);
	if(addr == m_manager1) {
		renew_r_hash_space();
		renew_w_hash_space();
	} else if(m_manager2.connectable() && addr == m_manager2) {
		renew_r_hash_space();
		renew_w_hash_space();
	}
}

void Server::lost_node(address addr, role_type id)
{
	// XXX
	LOG_WARN("lost node ",id," ",addr);
}


}  // namespace kumo

