#ifndef RPC_CLUSTER_H__
#define RPC_CLUSTER_H__

#include "rpc/client.h"
#include "rpc/protocol.h"
#include <mp/pthread.h>
#include <algorithm>
#include <iterator>

namespace rpc {


class cluster;

struct cluster_init_sender {
	cluster_init_sender(int fd, const address& addr, role_type id);
};

class cluster_transport : private cluster_init_sender,
		public basic_transport, public connection<cluster_transport> {
public:
	cluster_transport(int fd, basic_shared_session s, transport_manager* srv);
	cluster_transport(int fd, transport_manager* srv);
	~cluster_transport();

	typedef std::auto_ptr<msgpack::zone> auto_zone;

	void process_message(msgobj msg, auto_zone& z);

private:
	void rebind(basic_shared_session s);
	cluster* get_server();
	cluster* get_server(transport_manager* srv);

private:
	short m_role;

private:
	cluster_transport();
	cluster_transport(const cluster_transport&);
};


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
	void set_role(role_type role_id) { m_role = role_id; }
	friend class cluster_transport;
	short m_role;
};


typedef mp::shared_ptr<node> shared_node;
typedef mp::weak_ptr<node>   weak_node;


class cluster : protected client<cluster_transport, node> {
public:
	typedef client<cluster_transport, node> client_t;

	typedef std::auto_ptr<msgpack::zone> auto_zone;
	typedef rpc::msgobj      msgobj;
	typedef rpc::method_id   method_id;
	typedef rpc::msgid_t     msgid_t;
	typedef rpc::shared_zone shared_zone;
	typedef rpc::shared_node shared_node;
	typedef rpc::weak_node   weak_node;
	typedef rpc::role_type   role_type;

	cluster(role_type self_id,
			const address& self_addr,
			unsigned short connect_retry_limit,
			unsigned short connect_timeout_steps,
			unsigned int reconnect_timeout_msec = 5*1000);

	virtual ~cluster();

	// called when new node is connected.
	virtual void new_node(address addr, role_type id, shared_node n) { }

	// called when node is lost.
	virtual void lost_node(address addr, role_type id) { }

	virtual void cluster_dispatch(
			shared_node& from, role_type role, weak_responder response,
			method_id method, msgobj param, shared_zone& z) = 0;

	virtual void cluster_dispatch_request(
			basic_shared_session& s, role_type role,
			method_id method, msgobj param,
			msgid_t msgid, auto_zone& z);

public:
	// step timeout count.
	void step_timeout();

	// add accepted connection
	void accepted(int fd);

	// get/create RPC stub instance for the address.
	shared_node get_node(const address& addr);

	// apply function to all nodes whose id is role_id.
	// F is required to implement
	// void operator() (std::pair<address, shared_node>&);
	template <typename F>
	void for_each_node(role_type role_id, F f);

	// add unmanaged connection
	//template <typename Connection>
	//Connection* add(int fd);

	// return self address;
	const address& addr() const;

private:
	shared_node bind_session(const address& addr);

	friend class cluster_transport;

private:
	void transport_lost(shared_node& s);

private:
	role_type m_self_id;
	address m_self_addr;

	unsigned short m_connect_retry_limit;

private:
	virtual void dispatch(
			shared_node& from, weak_responder response,
			method_id method, msgobj param, shared_zone& z);

	virtual void dispatch_request(
			basic_shared_session& s, weak_responder response,
			method_id method, msgobj param, shared_zone& life);

private:
	cluster();
	cluster(const cluster&);
};


inline void cluster::step_timeout()
{
	client_t::step_timeout();
}

inline const address& cluster::addr() const
{
	return m_self_addr;
}


}  // namespace rpc

#endif /* rpc/cluster.h */

