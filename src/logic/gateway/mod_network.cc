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
#include "gateway/framework.h"
#include "gateway/mod_network.h"
#include "manager/mod_network.h"

namespace kumo {
namespace gateway {


RPC_IMPL(mod_network_t, HashSpacePush, req, z, response)
{
	LOG_DEBUG("HashSpacePush");

	{
		pthread_scoped_wrlock hslk(share->hs_rwlock());
		share->update_whs(req.param().wseed, hslk);
		share->update_rhs(req.param().rseed, hslk);
	}

	response.result(true);
}


void mod_network_t::renew_hash_space()
{
	shared_zone nullz;
	manager::mod_network_t::HashSpaceRequest param;

	rpc::callback_t callback( BIND_RESPONSE(mod_network_t, HashSpaceRequest) );

	net->get_session(share->manager1())->call(
			param, nullz, callback, 10);

	if(!share->manager2().connectable()) { return; }

	net->get_session(share->manager2())->call(
			param, nullz, callback, 10);
}

void mod_network_t::renew_hash_space_for(const address& addr)
{
	shared_session ns(net->get_session(addr));
	shared_zone nullz;
	manager::mod_network_t::HashSpaceRequest param;
	ns->call(param, nullz,
			BIND_RESPONSE(mod_network_t, HashSpaceRequest), 10);
}

void mod_network_t::keep_alive()
{
	shared_zone nullz;
	manager::mod_network_t::HashSpaceRequest param;
	shared_session ns;

	ns = net->get_session(share->manager1());
	if(!ns->is_bound()) {  // FIXME
		ns->call(param, nullz,
				BIND_RESPONSE(mod_network_t, HashSpaceRequest), 10);
	}

	if(!share->manager2().connectable()) { return; }

	ns = net->get_session(share->manager2());
	if(!ns->is_bound()) {  // FIXME
		ns->call(param, nullz,
				BIND_RESPONSE(mod_network_t, HashSpaceRequest), 10);
	}
}


RPC_REPLY_IMPL(mod_network_t, HashSpaceRequest, from, res, err, z)
{
	if(!err.is_nil()) {
		LOG_ERROR("HashSpaceRequest failed ",err);
		if(SESSION_IS_ACTIVE(from)) {
			shared_zone nullz;
			manager::mod_network_t::HashSpaceRequest param;

			from->call(param, nullz,
					BIND_RESPONSE(mod_network_t, HashSpaceRequest), 10);
		}  // retry on Gateway::session_lost() if the node is lost
	} else {
		gateway::mod_network_t::HashSpacePush st(res.convert());
		{
			pthread_scoped_wrlock hslk(share->hs_rwlock());
			share->update_whs(st.wseed, hslk);
			share->update_rhs(st.rseed, hslk);
		}
	}
}


}  // namespace gateway
}  // namespace kumo

