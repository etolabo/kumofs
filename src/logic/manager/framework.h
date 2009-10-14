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
#ifndef MANAGER_FRAMEWORK_H__
#define MANAGER_FRAMEWORK_H__

#include "logic/cluster_logic.h"
#include "logic/clock_logic.h"
#include "manager/mod_network.h"
#include "manager/mod_replace.h"
#include "manager/mod_control.h"

namespace kumo {
namespace manager {


class framework : public cluster_logic<framework>, public clock_logic {
public:
	template <typename Config>
	framework(const Config& cfg);

	template <typename Config>
	void run(const Config& cfg);

	void cluster_dispatch(
			shared_node from, weak_responder response,
			rpc::method_id method, rpc::msgobj param, auto_zone z);

	void subsystem_dispatch(
			shared_peer from, weak_responder response,
			rpc::method_id method, rpc::msgobj param, auto_zone z);

	void new_node(address addr, role_type id, shared_node n);
	void lost_node(address addr, role_type id);

	// rpc_server
	void keep_alive()
	{
		mod_network.keep_alive();
	}

	// override rpc_server<framework>::timer_handler
	void timer_handler()
	{
		clock_logic::clock_update_time();
		rpc_server<framework>::timer_handler();
	}

public:
	mod_network_t mod_network;
	mod_replace_t mod_replace;
	mod_control_t mod_control;

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

	// connected but not joined servers
	mp::pthread_mutex m_new_servers_mutex;
	new_servers_t m_new_servers;

	const address m_partner;

	bool m_cfg_auto_replace;
	const short m_cfg_replace_delay_seconds;

public:
	RESOURCE_ACCESSOR(mp::pthread_mutex, hs_mutex);
	RESOURCE_ACCESSOR(HashSpace, rhs);
	RESOURCE_ACCESSOR(HashSpace, whs);

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


template <typename Parameter>
struct for_each_call_t {
	for_each_call_t(Parameter& param, shared_zone& life,
			rpc::callback_t& callback, unsigned short timeout_steps) :
		m_param(param), m_life(life),
		m_callback(callback), m_timeout_steps(timeout_steps) { }
	void operator() (shared_node& n)
	{
		n->call(m_param, m_life, m_callback, m_timeout_steps);
	}
private:
	Parameter& m_param;
	shared_zone& m_life;
	rpc::callback_t& m_callback;
	unsigned short m_timeout_steps;
};

template <typename Parameter, typename F>
struct for_each_call_do_t {
	for_each_call_do_t(Parameter& param, shared_zone& life,
			rpc::callback_t& callback, unsigned short timeout_steps,
			F f) :
		m_param(param), m_life(life),
		m_callback(callback), m_timeout_steps(timeout_steps),
		m(f) { }
	void operator() (shared_node& n)
	{
		n->call(m_param, m_life, m_callback, m_timeout_steps);
		m(n);
	}
private:
	Parameter& m_param;
	shared_zone& m_life;
	rpc::callback_t& m_callback;
	unsigned short m_timeout_steps;
	F m;
};

template <typename Parameter>
for_each_call_t<Parameter> for_each_call(Parameter& param, shared_zone& life,
		rpc::callback_t& callback, unsigned short timeout_steps)
{
	return for_each_call_t<Parameter>(param, life, callback, timeout_steps);
}


template <typename Parameter, typename F>
for_each_call_do_t<Parameter, F> for_each_call_do(Parameter& param, shared_zone& life,
		rpc::callback_t& callback, unsigned short timeout_steps,
		F f)
{
	return for_each_call_do_t<Parameter, F>(param, life, callback, timeout_steps, f);
}


}  // namespace manager
}  // namespace kumo

#endif  /* manager/framework.h */

