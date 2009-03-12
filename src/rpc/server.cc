#include "rpc/server.h"
#include "rpc/protocol.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

namespace rpc {


server::server() { }

server::~server() { }


shared_peer server::accepted(int fd)
{
#ifdef USE_TCP_NODELAY
	// XXX
	int on = 1;
	::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));  // ignore error
#endif
	basic_shared_session s(new peer(this));
	wavy::add<transport>(fd, s, this);
	void* k = (void*)s.get();

	pthread_scoped_lock lk(m_peers_mutex);
	m_peers.insert( peers_t::value_type(k, basic_weak_session(s)) );
	return mp::static_pointer_cast<peer>(s);
}


void server::dispatch_request(
		basic_shared_session& s, weak_responder response,
		method_id method, msgobj param, auto_zone z)
{
	dispatch(mp::static_pointer_cast<peer>(s),
			response, method, param, z);
}


void server::step_timeout()
{
	pthread_scoped_lock lk(m_peers_mutex);
	for(peers_t::iterator it(m_peers.begin()), it_end(m_peers.end());
			it != it_end; ) {
		basic_shared_session p(it->second.lock());
		if(p && !p->is_lost()) {
			wavy::submit(&basic_session::step_timeout, p.get(), p);
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
	{
		pthread_scoped_lock lk(m_peers_mutex);
		m_peers.erase(k);
	}

	s->force_lost(res, err);
}


}  // namespace rpc

