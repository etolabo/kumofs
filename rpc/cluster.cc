#include "rpc/cluster.h"
#include "rpc/sbuffer.h"

namespace rpc {


cluster_init_sender::cluster_init_sender() { }

inline cluster_init_sender::cluster_init_sender(int fd, const address& addr, role_type id)
{
	send_init(fd, addr, id);
}

inline void cluster_init_sender::send_init(int fd, const address& addr, role_type id)
{
	sbuffer buf;
	rpc_initmsg param(addr, id);
	msgpack::pack(buf, param);
	mp::iothreads::send_data(fd, (char*)buf.data(), buf.size(),
			&mp::iothreads::writer::finalize_free,
			buf.data());
	buf.release();
	LOG_TRACE("sent init message: ",addr);
}


cluster_transport::cluster_transport(int fd, basic_shared_session s, transport_manager* srv) :
	cluster_init_sender(fd, get_server(srv)->m_self_addr, get_server(srv)->m_self_id),
	basic_transport(s, srv),
	connection<cluster_transport>(fd),
	m_role(PEER_NOT_SET)
{
	s->bind_transport(connection<cluster_transport>::fd());
}

cluster_transport::cluster_transport(int fd, transport_manager* srv) :
	//cluster_init_sender(fd, get_server(srv)->m_self_addr, get_server(srv)->m_self_id),
	basic_transport(basic_shared_session(), srv),
	connection<cluster_transport>(fd),
	m_role(PEER_NOT_SET)
{ }

cluster_transport::~cluster_transport()
{
	if(m_session) {
		m_session->unbind_transport(connection<cluster_transport>::fd(), m_session);
	}
}

void cluster_transport::rebind(basic_shared_session s)
{
	if(m_session) {
		m_session->unbind_transport(connection<cluster_transport>::fd(), m_session);
	}
	m_session = s;
	s->bind_transport(connection<cluster_transport>::fd());
}


cluster* cluster_transport::get_server()
	{ return get_server(get_manager()); }

cluster* cluster_transport::get_server(transport_manager* srv)
	{ return static_cast<cluster*>(srv); }


node::node(session_manager* mgr) :
	session(mgr),
	m_role(-1) { }

node::~node() { }


void cluster_transport::process_message(msgobj msg, auto_zone& z)
{
	// TODO self-modifying code
	if(m_role == PEER_NOT_SET) {
		rpc_initmsg init;
		try {
			init = msg.convert();
		} catch (msgpack::type_error&) {
			m_role = PEER_SERVER;
		}

		if(m_role == PEER_NOT_SET) {
			// cluster node
			LOG_TRACE("receive init message: ",(uint16_t)init.role_id()," ",init.addr());

			if(!m_session) {
				if(!init.addr().connectable()) {
					throw std::runtime_error("invalid address");
				}
				send_init(fd(), get_server()->m_self_addr, get_server()->m_self_id);
				rebind( get_server()->bind_session(init.addr()) );
			}

			m_role = init.role_id();

			node* n = static_cast<node*>(m_session.get());
			if(!n->is_role_set()) {
				// new node
				n->set_role(m_role);
				shared_node n(mp::static_pointer_cast<node>(m_session));
				mp::iothreads::submit(
						&cluster::new_node, get_server(),
						init.addr(), m_role, n);
			}
			return;

		} else {
			if(m_session) { throw msgpack::type_error(); }
			// servcer node
			cluster::subsys* sub =
					static_cast<cluster::subsys*>(&get_server()->subsystem());
			basic_shared_session s(new peer(sub));
			rebind(s);
			mp::iothreads::submit(
					&cluster::subsys::add_session, sub, s);
		}
	}

	// cluster node
	LOG_TRACE("receive rpc message: ",msg);
	rpc_message rpc(msg.convert());
	if(rpc.is_request()) {
		rpc_request<msgobj> msgreq(rpc);
		if(m_role == PEER_SERVER) {
			get_server()->subsystem_dispatch_request(
					m_session,
					msgreq.method(), msgreq.param(), msgreq.msgid(), z);
		} else {
			get_server()->cluster_dispatch_request(
					m_session, m_role,
					msgreq.method(), msgreq.param(), msgreq.msgid(), z);
		}

	} else {
		rpc_response<msgobj, msgobj> msgres(rpc);
		basic_transport::process_response(
				msgres.result(), msgres.error(), msgres.msgid(), z);
	}
}


cluster::cluster(role_type self_id,
		const address& self_addr,
		unsigned short connect_retry_limit,
		unsigned short connect_timeout_steps,
		unsigned int reconnect_timeout_msec) :
	client_t(connect_timeout_steps, reconnect_timeout_msec),
	m_self_id(self_id),
	m_self_addr(self_addr),
	m_connect_retry_limit(connect_retry_limit),
	m_subsystem(this) { }

cluster::~cluster() { }

void cluster::accepted(int fd)
{
	mp::iothreads::add<cluster_transport>(fd, (client_t*)this);
}


shared_node cluster::get_node(const address& addr)
{
	shared_node n( get_session(addr) );
	if(!n->is_role_set()) { n->m_addr = addr; }
	return n;
}

inline shared_node cluster::bind_session(const address& addr)
{
	shared_node n( create_session(addr) );
	n->m_addr = addr;
	return n;
}

void cluster::transport_lost(shared_node& n)
{
	//if(n->empty()) {
	//	LOG_DEBUG("empty session ",addr);

	//	client_t::transport_lost(addr, n);

	//	node* n = static_cast<node*>(n.get());
	//	if(n->is_role_set()) {
	//		// node is lost
	//		lost_node(n->addr(), n->role());
	//	}

	//} else {
		if(n->connect_retried_count() > m_connect_retry_limit) {
			LOG_DEBUG("give up to reconnect ",n->addr());

			msgpack::object res;
			res.type = msgpack::type::NIL;
			msgpack::object err;
			err.type = msgpack::type::POSITIVE_INTEGER;
			err.via.u64 = protocol::NODE_LOST_ERROR;

			n->force_lost(res, err);

			if(n->is_role_set()) {
				// node is lost
				lost_node(n->addr(), n->role());
			}

		} else {
			LOG_DEBUG("reconnect to ",n->addr());
			connect_session(n->addr(), n);
		}
	//}
}



void cluster::cluster_dispatch_request(
		basic_shared_session& s, role_type role,
		method_id method, msgobj param,
		msgid_t msgid, auto_zone& z)
{
	shared_zone life(new mp::zone());
	life->push_finalizer(
			&mp::object_delete<msgpack::zone>,
			z.get());
	z.release();
	weak_responder response(s, msgid);
	shared_node from = mp::static_pointer_cast<node>(s);
	cluster_dispatch(from, role, response, method, param, life);
}


void cluster::subsystem_dispatch_request(
		basic_shared_session& s,
		method_id method, msgobj param,
		msgid_t msgid, auto_zone& z)
{
	shared_zone life(new mp::zone());
	life->push_finalizer(
			&mp::object_delete<msgpack::zone>,
			z.get());
	z.release();
	weak_responder response(s, msgid);
	shared_peer from = mp::static_pointer_cast<peer>(s);
	subsystem_dispatch(from, response, method, param, life);
}


void cluster::dispatch_request(
		basic_shared_session& s, weak_responder response,
		method_id method, msgobj param, shared_zone& life)
{
	// connection<IMPL>::process_message is hooked.
	// transport<IMPL>::process_request won't be called.
	throw std::logic_error("cluster::dispatch_request called");
}

void cluster::dispatch(
		shared_node& from, weak_responder response,
		method_id method, msgobj param, shared_zone& life)
{
	throw std::logic_error("cluster::dispatch called");
}


cluster::subsys::subsys(cluster* srv) :
	m_srv(srv) { }

cluster::subsys::~subsys() { }

void cluster::subsys::add_session(basic_shared_session s)
{
	void* k = (void*)s.get();
	m_peers.insert( peers_t::value_type(k, basic_weak_session(s)) );
}

void cluster::subsys::dispatch(
		shared_peer& from, weak_responder response,
		method_id method, msgobj param, shared_zone& life)
{
	throw std::logic_error("cluster::subsys::dispatch called");
}


}  // namespace rpc

