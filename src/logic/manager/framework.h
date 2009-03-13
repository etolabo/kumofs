#ifndef MANAGER_FRAMEWORK_H__
#define MANAGER_FRAMEWORK_H__

#include "logic/cluster_logic.h"
#include "logic/clock_logic.h"
#include "manager/proto_network.h"
#include "manager/proto_replace.h"
#include "manager/control_framework.h"


#define EACH_ACTIVE_SERVERS_BEGIN(NODE) \
	for(servers_t::iterator _it_(share->servers().begin()), it_end(share->servers().end()); \
			_it_ != it_end; ++_it_) { \
		shared_node NODE(_it_->second.lock()); \
		if(SESSION_IS_ACTIVE(NODE)) {
			// FIXME share->servers().erase(it) ?

#define EACH_ACTIVE_SERVERS_END \
		} \
	}


namespace kumo {
namespace manager {


class framework : public cluster_logic<framework>, public clock_logic {
public:
	template <typename Config>
	framework(const Config& cfg);

	void cluster_dispatch(
			shared_node from, weak_responder response,
			rpc::method_id method, rpc::msgobj param, auto_zone z);

	void subsystem_dispatch(
			shared_peer from, weak_responder response,
			rpc::method_id method, rpc::msgobj param, auto_zone z);

	void new_node(address addr, role_type id, shared_node n);
	void lost_node(address addr, role_type id);

	// cluster_logic
	void keep_alive()
	{
		scope_proto_network().keep_alive();
	}

private:
	proto_network m_proto_network;
	proto_replace m_proto_replace;

public:
	proto_network& scope_proto_network() { return m_proto_network; }
	proto_replace& scope_proto_replace() { return m_proto_replace; }

private:
	control_framework m_control_framework;

private:
	framework();
	framework(const framework&);
};


typedef std::vector<weak_node> new_servers_t;
typedef std::map<address, weak_node> servers_t;


class resource {
public:
	template <typename Config>
	resource(const Config& cfg);

private:
	mp::pthread_mutex m_hs_mutex;
	HashSpace m_rhs;
	HashSpace m_whs;

	// 'joined servers' including both active and fault servers
	mp::pthread_mutex m_servers_mutex;
	servers_t m_servers;

	// added but 'not joined servers'
	mp::pthread_mutex m_new_servers_mutex;
	new_servers_t m_new_servers;

	const address m_partner;

	bool m_cfg_auto_replace;
	const short m_cfg_replace_delay_seconds;

public:
	RESOURCE_ACCESSOR(mp::pthread_mutex, hs_mutex);
	RESOURCE_ACCESSOR(HashSpace, rhs);
	RESOURCE_ACCESSOR(HashSpace, whs);

	RESOURCE_ACCESSOR(mp::pthread_mutex, servers_mutex);
	RESOURCE_ACCESSOR(servers_t, servers);

	RESOURCE_ACCESSOR(mp::pthread_mutex, new_servers_mutex);
	RESOURCE_ACCESSOR(new_servers_t, new_servers);

	RESOURCE_CONST_ACCESSOR(address, partner);

	RESOURCE_ACCESSOR(bool, cfg_auto_replace);
	RESOURCE_CONST_ACCESSOR(short, cfg_replace_delay_seconds);

private:
	resource();
	resource(const resource&);
};


extern std::auto_ptr<framework> net;
extern std::auto_ptr<resource> share;


}  // namespace manager
}  // namespace kumo

#endif  /* manager/framework.h */

