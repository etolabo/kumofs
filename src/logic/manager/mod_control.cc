//
// kumofs
//
// Copyright (C) 2009 Etolabo Corp.
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
#include "manager/framework.h"
#include "server/mod_control.h"

namespace kumo {
namespace manager {


mod_control_t::mod_control_t() { }
mod_control_t::~mod_control_t() { }


RPC_IMPL(mod_control_t, GetNodesInfo, req, z, response)
{
	Status res;

	{
		pthread_scoped_lock hslk(share->hs_mutex());
		res.hsseed() = HashSpace::Seed(share->whs());
	}

	pthread_scoped_lock nslk(share->new_servers_mutex());
	for(new_servers_t::iterator it(share->new_servers().begin()), it_end(share->new_servers().end());
			it != it_end; ++it) {
		shared_node n(it->lock());
		if(n) {
			res.newcomers().push_back(n->addr());
		}
	}
	nslk.unlock();

	response.result(res);
}

RPC_IMPL(mod_control_t, AttachNewServers, req, z, response)
{
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		net->mod_replace.attach_new_servers(hslk);
		net->mod_replace.start_replace(hslk);
	}
	response.null();
}

RPC_IMPL(mod_control_t, DetachFaultServers, req, z, response)
{
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		net->mod_replace.detach_fault_servers(hslk);
		net->mod_replace.start_replace(hslk);
	}
	response.null();
}

RPC_IMPL(mod_control_t, CreateBackup, req, z, response)
{
	if(req.param().suffix.empty()) {
		std::string msg("empty suffix");
		response.error(msg);
		return;
	}
	server::mod_control_t::CreateBackup param(req.param().suffix);
	rpc::callback_t callback( BIND_RESPONSE(mod_control_t, CreateBackup) );
	shared_zone nullz;

	net->for_each_node(ROLE_SERVER,
			for_each_call(param, nullz, callback, 10));

	response.null();
}

RPC_REPLY_IMPL(mod_control_t, CreateBackup, from, res, err, z)
{ }

RPC_IMPL(mod_control_t, SetAutoReplace, req, z, response)
{
	if(share->cfg_auto_replace() && !req.param().enable) {
		share->cfg_auto_replace() = false;
		response.result(false);
	} else if(!share->cfg_auto_replace() && req.param().enable) {
		share->cfg_auto_replace() = true;

		{
			pthread_scoped_lock hslk(share->hs_mutex());
			net->mod_replace.attach_new_servers(hslk);
			net->mod_replace.detach_fault_servers(hslk);
		}

		response.result(true);
	}
	response.null();
}

RPC_IMPL(mod_control_t, StartReplace, req, z, response)
{
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		net->mod_replace.start_replace(hslk, req.param().full);
	}

	response.null();
}


}  // namespace manager
}  // namespace kumo

