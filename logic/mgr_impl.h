#ifndef LOGIC_MGR_IMPL_H__
#define LOGIC_MGR_IMPL_H__

#include "logic/mgr.h"

namespace kumo {


#define BIND_RESPONSE(FUNC, ...) \
	mp::bind(&Manager::FUNC, this, _1, _2, _3, _4, ##__VA_ARGS__)


#define RPC_FUNC(NAME, from, response, z, param) \
	RPC_IMPL(Manager, NAME, from, response, z, param)


#define CLUSTER_FUNC(NAME, from, response, z, param) \
	CLUSTER_IMPL(Manager, NAME, from, response, z, param)


#define RPC_REPLY(NAME, from, res, err, life, ...) \
	RPC_REPLY_IMPL(Manager, NAME, from, res, err, life, ##__VA_ARGS__)


#define EACH_ACTIVE_SERVERS_BEGIN(NODE) \
	for(servers_t::iterator _it_(m_servers.begin()), it_end(m_servers.end()); \
			_it_ != it_end; ++_it_) { \
		shared_node NODE(_it_->second.lock()); \
		if(SESSION_IS_ACTIVE(NODE)) {
			// FIXME m_servers.erase(it) ?

#define EACH_ACTIVE_SERVERS_END \
		} \
	}


}  // namespace kumo

#endif /* logic/mgr_impl.h */

