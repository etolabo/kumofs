#include "logic/gw_impl.h"
#include "gateway/gateway.h"

namespace kumo {


Gateway::~Gateway()
{
}


void Gateway::dispatch(
		shared_session from, weak_responder response,
		method_id method, msgobj param, auto_zone z)
{
	switch(method) {
	RPC_DISPATCH(HashSpacePush);
	default:
		throw std::runtime_error("unknown method");
	}
}

void Gateway::submit(get_request& req)
{
	wavy::submit(
			&Gateway::Get, this,
			req.callback, req.user, req.life,
			req.key, req.keylen);
}

void Gateway::submit(set_request& req)
{
	wavy::submit(
			&Gateway::Set, this,
			req.callback, req.user, req.life,
			req.key, req.keylen,
			req.val, req.vallen);
}

void Gateway::submit(delete_request& req)
{
	wavy::submit(
			&Gateway::Delete, this,
			req.callback, req.user, req.life,
			req.key, req.keylen);
}

void Gateway::add_gateway(GatewayInterface* gw)
{
	gw->listen(this);
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


void Gateway::incr_error_count()
{
	LOG_DEBUG("increment error count ",m_error_count);
	if(m_error_count >= m_cfg_renew_threshold) {
		m_error_count = 0;
		renew_hash_space();
		sleep(1);   // FIXME ad-hoc delay
	} else {
		++m_error_count;
	}
}


}  // namespace kumo

