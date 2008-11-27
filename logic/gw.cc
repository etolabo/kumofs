#include "logic/gw_impl.h"

namespace kumo {


Gateway::~Gateway()
{
}


/*
Gateway::Transport::Transport(int fd, shared_session s, transport_manager* mgr) :
	rpc::transport<Transport>(fd, s, mgr) { }

Gateway::Transport::Transport(int fd, transport_manager* mgr) :
	rpc::transport<Transport>(fd, mgr) { }

Gateway::Transport::~Transport() { }


void Gateway::Transport::dispatch_request(method_id method, msgobj param, responder& response, auto_zone& z)
{
	shared_zone life(new mp::zone());
	life->push_finalizer(&mp::object_delete<msgpack::zone>, z.get());
	z.release();
	reinterpret_cast<Gateway*>(get_manager())->dispatch(
			m_session, method, param, response, life);
}
*/

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

