#ifndef LOGIC_MGR_IMPL_H__
#define LOGIC_MGR_IMPL_H__

#include "logic/mgr.h"

namespace kumo {


#define BIND_RESPONSE(FUNC, ...) \
	mp::bind(&Manager::FUNC, this, _1, _2, _3, _4, ##__VA_ARGS__)


#define RPC_FUNC(NAME, from, response, life, param) \
	RPC_IMPL(Manager, NAME, from, response, life, param)


#define CLUSTER_FUNC(NAME, from, response, life, param) \
	CLUSTER_IMPL(Manager, NAME, from, response, life, param)


#define CLISRV_FUNC(NAME, from, response, life, param) \
	CLISRV_IMPL(Manager, NAME, from, response, life, param)


#define RPC_REPLY(NAME, from, res, err, life, ...) \
	RPC_REPLY_IMPL(Manager, NAME, from, res, err, life, ##__VA_ARGS__)


#define EACH_ACTIVE_SERVERS_BEGIN(NODE) \
	for(servers_t::iterator _it_(m_servers.begin()), it_end(m_servers.end()); \
			_it_ != it_end; ++_it_) { \
		shared_node NODE(_it_->second.lock()); \
		if(NODE && !NODE->is_lost()) {
			// FIXME m_servers.erase(it) ?

#define EACH_ACTIVE_SERVERS_END \
		} \
	}


#define EACH_ACTIVE_NEW_COMERS_BEGIN(NODE) \
	for(newcomer_servers_t::iterator _it_(m_newcomer_servers.begin()), it_end(m_newcomer_servers.end()); \
			_it_ != it_end; ++_it_) { \
		shared_node NODE(_it_->lock()); \
		if(NODE && !NODE->is_lost()) {
			// FIXME m_newcomer_servers.erase(it) ?

#define EACH_ACTIVE_NEW_COMERS_END \
		} \
	}

#define EACH_CLIENTS_BEGIN(NODE) \
	for(clients_t::iterator _it_(m_clients.begin()), it_end(m_clients.end()); \
			_it_ != it_end; ++_it_) { \
		shared_node NODE(get_node((*_it_)->addr(), protocol::::CLIENT));

#define EACH_CLIENTS_END \
	}


}  // namespace kumo

#endif /* logic/mgr_impl.h */

