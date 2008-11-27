#include "rpc/server.h"
#include "rpc/protocol.h"

namespace rpc {


server::server() { }
server::~server() { }


server_transport::server_transport(
		int fd, basic_shared_session& s, transport_manager* mgr) :
	basic_transport(s, mgr),
	connection<server_transport>(fd)
{
	m_session->bind_transport(connection<server_transport>::fd());
}

server_transport::~server_transport()
{
	m_session->unbind_transport(connection<server_transport>::fd(), m_session);
}

void server::dispatch_request(
		basic_shared_session& s, weak_responder response,
		method_id method, msgobj param, shared_zone& life)
{
	shared_peer from = mp::static_pointer_cast<peer>(s);
	dispatch(from, response, method, param, life);
}

void server::accepted(int fd)
{
	basic_shared_session s(new peer(this));
	mp::iothreads::add<server_transport>(fd, s, this);
	void* k = (void*)s.get();
	m_peers.insert( peers_t::value_type(k, basic_weak_session(s)) );
}


void server::step_timeout()
{
	for(peers_t::iterator it(m_peers.begin()), it_end(m_peers.end());
			it != it_end; ) {
		basic_shared_session p(it->second.lock());
		if(p && !p->is_lost()) {
			p->step_timeout(p);
			++it;
		} else {
			m_peers.erase(it++);
		}
	}
}

void server::transport_lost_notify(basic_shared_session& s)
{
	msgpack::object res;
	res.type = msgpack::type::NIL;
	msgpack::object err;
	err.type = msgpack::type::POSITIVE_INTEGER;
	err.via.u64 = protocol::NODE_LOST_ERROR;

	void* k = (void*)s.get();
	m_peers.erase(k);

	s->force_lost(res, err);
}


}  // namespace rpc

