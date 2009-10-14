//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
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
		(this->*m_process_state)(msg, z.release());
		//wavy::submit(m_process_state,
		//		shared_self<cluster_transport>(),
		//		msg, z.get());
		//z.release();
	}
}


class cluster : protected client_tmpl<cluster_transport, node> {
public:
	typedef client_tmpl<cluster_transport, node> client_base;

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

	// apply function to all connected sessions.
	// F is required to implement
	// void operator() (shared_session);
	template <typename F>
	void for_each_node(role_type role_id, F f);

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
	client_base::step_timeout();
	m_subsystem.step_timeout();
}

inline const address& cluster::addr() const
{
	return m_self_addr;
}

namespace detail {
	template <typename F>
	struct cluster_if_role {
		cluster_if_role(role_type role_id, F f) : role(role_id), m(f) { }
		inline void operator() (shared_node& n)
		{
			if(n->role() == role) { m(n); }
		}
	private:
		role_type role;
		F m;
		cluster_if_role();
	};
}  // namespace detail

template <typename F>
void cluster::for_each_node(role_type role_id, F f)
{
	for_each_session(detail::cluster_if_role<F>(role_id, f));
}

inline server& cluster::subsystem()
{
	return static_cast<server&>(m_subsystem);
}



template <typename Parameter>
class request<Parameter, rpc::node> {
public:
	request(shared_node from, msgobj param) :
		m_from(from)
	{
		param.convert(&m_param);
	}

public:
	Parameter& param()
	{
		return m_param;
	}

	const Parameter& param() const
	{
		return m_param;
	}

	shared_node& node()
	{
		return m_from;
	}

private:
	Parameter m_param;
	shared_node m_from;

private:
	request();
	request(const request<Parameter, rpc::node>&);
};


}  // namespace rpc

#endif /* rpc/cluster.h */

