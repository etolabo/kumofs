#include "gateway/framework.h"

namespace kumo {
namespace gateway {


std::auto_ptr<framework> net;
std::auto_ptr<resource> share;


void framework::dispatch(
		shared_session from, weak_responder response,
		rpc::method_id method, rpc::msgobj param, auto_zone z)
{
	switch(method.get()) {
	RPC_DISPATCH(proto_network, HashSpacePush_1);
	default:
		// FIXME exception class
		throw std::runtime_error("unknown method");
	}
}


void framework::step_timeout()
{
	rpc::client<>::step_timeout();
}


void framework::session_lost(const address& addr, shared_session& s)
{
	LOG_INFO("lost session ",addr);
	if(addr == share->manager1() || addr == share->manager2()) {
		m_proto_network.renew_hash_space_for(addr);
	}
}


inline void framework::submit(get_request& req)
{
	m_scope_store.Get(req.callback, req.user, req.life,
			req.key, req.keylen, req.hash);
}

inline void framework::submit(set_request& req)
{
	m_scope_store.Set(req.callback, req.user, req.life,
			req.key, req.keylen, req.hash,
			req.val, req.vallen);
}

inline void framework::submit(delete_request& req)
{
	m_scope_store.Delete(req.callback, req.user, req.life,
			req.key, req.keylen, req.hash);
}


// interface.h:
void submit(get_request& req)
{
	net->submit(req);
}

// interface.h:
void submit(set_request& req)
{
	net->submit(req);
}

// interface.h:
void submit(delete_request& req)
{
	net->submit(req);
}


}  // namespace gateway
}  // namespace kumo

