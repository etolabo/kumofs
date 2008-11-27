#include "logic/gw_impl.h"

namespace kumo {


Gateway::~Gateway()
{
}


void Gateway::dispatch(
		shared_session& from, rpc::weak_responder response,
		method_id method, msgobj param, shared_zone& life)
{
	switch(method) {
	RPC_DISPATCH(HashSpacePush);
	default:
		throw std::runtime_error("unknown method");
	}
}

void Gateway::submit(get_request& req)
{
	mp::iothreads::submit(
			&Gateway::Get, this,
			req.callback, req.user, req.life,
			req.key, req.keylen);
}

void Gateway::submit(set_request& req)
{
	mp::iothreads::submit(
			&Gateway::Set, this,
			req.callback, req.user, req.life,
			req.key, req.keylen,
			req.val, req.vallen);
}

void Gateway::submit(delete_request& req)
{
	mp::iothreads::submit(
			&Gateway::Delete, this,
			req.callback, req.user, req.life,
			req.key, req.keylen);
}


void Gateway::step_timeout()
{
	rpc::client<>::step_timeout();
}


void Gateway::session_lost(const address& addr, shared_session& s)
{
	LOG_INFO("lost session ",addr);
	if(addr == m_manager1 || addr == m_manager2) {
		renew_hash_space_for(addr);
	}
}


}  // namespace kumo

