#include "rpc/cluster.h"
#include "rpc/sbuffer.h"

namespace rpc {


cluster_init_sender::cluster_init_sender(int fd, const address& addr, role_type id)
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
	m_role(-1)
{
	s->bind_transport(connection<cluster_transport>::fd());
}

cluster_transport::cluster_transport(int fd, transport_manager* srv) :
	cluster_init_sender(fd, get_server(srv)->m_self_addr, get_server(srv)->m_self_id),
	basic_transport(basic_shared_session(), srv),
	connection<cluster_transport>(fd),
	m_role(-1)
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
	if(m_role < 0) {
		rpc_initmsg init(msg.convert());
		LOG_TRACE("receive init message: ",(uint16_t)init.role_id()," ",init.addr());

		if(!m_session) {
			if(!init.addr().connectable()) {
				throw std::runtime_error("invalid address");
			}
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
	}

	LOG_TRACE("receive rpc message: ",msg);
	rpc_message rpc(msg.convert());
	if(rpc.is_request()) {
		rpc_request<msgobj> msgreq(rpc);
		get_server()->cluster_dispatch_request(
				m_session, m_role,
				msgreq.method(), msgreq.param(), msgreq.msgid(), z);

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
	m_connect_retry_limit(connect_retry_limit) { }

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
	cluster_dispatch(from, from->role(), response, method, param, life);
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
		method_id method, msgobj param, shared_zone& z)
{
	throw std::logic_error("cluster::dispatch called");
}


}  // namespace rpc

