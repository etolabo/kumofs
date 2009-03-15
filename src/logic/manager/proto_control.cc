#include "manager/framework.h"
#include "server/proto_control.h"

namespace kumo {
namespace manager {


proto_control::proto_control() { }
proto_control::~proto_control() { }


RPC_IMPL(proto_control, GetNodesInfo_1, req, z, response)
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

RPC_IMPL(proto_control, AttachNewServers_1, req, z, response)
{
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		net->scope_proto_replace().attach_new_servers(hslk);
		net->scope_proto_replace().start_replace(hslk);
	}
	response.null();
}

RPC_IMPL(proto_control, DetachFaultServers_1, req, z, response)
{
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		net->scope_proto_replace().detach_fault_servers(hslk);
		net->scope_proto_replace().start_replace(hslk);
	}
	response.null();
}

RPC_IMPL(proto_control, CreateBackup_1, req, z, response)
{
	if(req.param().suffix.empty()) {
		std::string msg("empty suffix");
		response.error(msg);
		return;
	}
	server::proto_control::CreateBackup_1 param(req.param().suffix);
	rpc::callback_t callback( BIND_RESPONSE(proto_control, CreateBackup_1) );
	shared_zone nullz;

	pthread_scoped_lock sslk(share->servers_mutex());
	EACH_ACTIVE_SERVERS_BEGIN(node)
		node->call(param, nullz, callback, 10);
	EACH_ACTIVE_SERVERS_END
	sslk.unlock();

	response.null();
}

RPC_REPLY_IMPL(proto_control, CreateBackup_1, from, res, err, life)
{ }

RPC_IMPL(proto_control, SetAutoReplace_1, req, z, response)
{
	if(share->cfg_auto_replace() && !req.param().enable) {
		share->cfg_auto_replace() = false;
		response.result(false);
	} else if(!share->cfg_auto_replace() && req.param().enable) {
		share->cfg_auto_replace() = true;

		{
			pthread_scoped_lock hslk(share->hs_mutex());
			net->scope_proto_replace().attach_new_servers(hslk);
			net->scope_proto_replace().detach_fault_servers(hslk);
		}

		response.result(true);
	}
	response.null();
}

RPC_IMPL(proto_control, StartReplace_1, req, z, response)
{
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		net->scope_proto_replace().start_replace(hslk);
	}

	response.null();
}


}  // namespace manager
}  // namespace kumo

