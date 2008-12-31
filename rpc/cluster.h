#ifndef RPC_CLUSTER_H__
#define RPC_CLUSTER_H__

#include "rpc/client.h"
#include "rpc/server.h"
#include <mp/pthread.h>
#include <algorithm>
#include <iterator>

namespace rpc {


class cluster;
class cluster_transport;


class node : public session {
public:
	node(session_manager* mgr);
	~node();

public:
	const address& addr() const { return m_addr; }
	bool is_role_set() const { return m_role >= 0; }
	role_type role() const { return m_role; }

private:
	address m_addr;
	friend class cluster;

private:
	inline bool set_role(role_type role_id);
	friend class cluster_transport;
	short m_role;

private:
	node();
	node(const node&);
};

typedef mp::shared_ptr<node> shared_node;
typedef mp::weak_ptr<node>   weak_node;


class cluster_transport : public basic_transport, public connection<cluster_transport> {
public:
	// cluster::get_node
	cluster_transport(int fd, basic_shared_session s, transport_manager* srv);

	// cluster::accepted
	cluster_transport(int fd, transport_manager* srv);

	~cluster_transport();

	void submit_message(msgobj msg, auto_zone& z);

private:
	void send_init();
	void rebind(basic_shared_session s);
	cluster* get_server();
	cluster* get_server(transport_manager* srv);

private:
	static const short PEER_NOT_SET = -1;
	static const short PEER_SERVER  = -2;

	void (cluster_transport::*m_process_state)(msgobj msg, msgpack::zone* newz);

	void init_message(msgobj msg, auto_zone z);
	void subsys_state(msgobj msg, msgpack::zone* newz);
	void cluster_state(msgobj msg, msgpack::zone* newz);

private:
	cluster_transport();
	cluster_transport(const cluster_transport&);
};

inline void cluster_transport::submit_message(msgobj msg, auto_zone& z)
{
	if(!m_process_state) {
		init_message(msg, z);
	} else {
		// FIXME better performance?
		//(this->*m_process_state)(msg, z.release());
		wavy::submit(m_process_state,
				shared_self<cluster_transport>(),
				msg, z.get());
		z.release();
	}
}


class cluster : protected client<cluster_transport, node> {
public:
	typedef client<cluster_transport, node> client_t;

	typedef rpc::shared_peer shared_session;
	typedef rpc::weak_peer   weak_session;

	cluster(role_type self_id,
			const address& self_addr,
			unsigned int connect_timeout_msec,
			unsigned short connect_retry_limit);

	virtual ~cluster();

	// called when new node is connected.
	virtual void new_node(address addr, role_type id, shared_node n) { }

	// called when node is lost.
	virtual void lost_node(address addr, role_type id) { }


	virtual void cluster_dispatch(
			shared_node from, weak_responder response,
			method_id method, msgobj param, auto_zone z) = 0;

	virtual void subsystem_dispatch(
			shared_peer from, weak_responder response,
			method_id method, msgobj param, auto_zone z)
	{
		throw msgpack::type_error();
	}

public:
	// step timeout count.
	void step_timeout();

	// add accepted connection
	void accepted(int fd);

	// get/create RPC stub instance for the address.
	shared_node get_node(const address& addr);

	// return self address;
	const address& addr() const;

	// get server interface.
	// it manages non-cluster clients.
	server& subsystem();

private:
	void transport_lost(shared_node& s);

private:
	role_type m_self_id;
	address m_self_addr;
	friend class cluster_transport;

private:
	virtual void dispatch(
			shared_node from, weak_responder response,
			method_id method, msgobj param, auto_zone z);

private:
	class subsys : public server {
	public:
		subsys(cluster* srv);
		~subsys();

	public:
		void dispatch(
				shared_peer from, weak_responder response,
				method_id method, msgobj param, auto_zone z);

		basic_shared_session add_session();

	private:
		cluster* m_srv;

	private:
		subsys();
		subsys(const subsys&);
	};

	subsys m_subsystem;

private:
	cluster();
	cluster(const cluster&);
};


inline void cluster::step_timeout()
{
	client_t::step_timeout();
	m_subsystem.step_timeout();
}

inline const address& cluster::addr() const
{
	return m_self_addr;
}

inline server& cluster::subsystem()
{
	return static_cast<server&>(m_subsystem);
}


}  // namespace rpc

#endif /* rpc/cluster.h */

