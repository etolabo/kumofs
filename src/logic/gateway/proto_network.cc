#include "gateway/framework.h"
#include "gateway/proto_network.h"
#include "manager/proto_network.h"

namespace kumo {
namespace gateway {


RPC_IMPL(proto_network, HashSpacePush, req, z, response)
{
	LOG_DEBUG("HashSpacePush");

	{
		pthread_scoped_wrlock hslk(share->hs_rwlock());
		share->update_whs(req.param().wseed, hslk);
		share->update_rhs(req.param().rseed, hslk);
	}

	response.result(true);
}


void proto_network::renew_hash_space()
{
	shared_zone nullz;
	manager::proto_network::HashSpaceRequest param;

	rpc::callback_t callback( BIND_RESPONSE(proto_network, HashSpaceRequest) );

	net->get_server(share->manager1())->call(
			param, nullz, callback, 10);

	if(share->manager2().connectable()) {
		net->get_server(share->manager2())->call(
				param, nullz, callback, 10);
	}
}

void proto_network::renew_hash_space_for(const address& addr)
{
	shared_session ns(net->get_server(addr));
	shared_zone nullz;
	manager::proto_network::HashSpaceRequest param;
	ns->call(param, nullz,
			BIND_RESPONSE(proto_network, HashSpaceRequest), 10);
}

RPC_REPLY_IMPL(proto_network, HashSpaceRequest, from, res, err, life)
{
	if(!err.is_nil()) {
		LOG_DEBUG("HashSpaceRequest failed ",err);
		if(SESSION_IS_ACTIVE(from)) {
			shared_zone nullz;
			manager::proto_network::HashSpaceRequest param;

			from->call(param, nullz,
					BIND_RESPONSE(proto_network, HashSpaceRequest), 10);
		}  // retry on Gateway::session_lost() if the node is lost
	} else {
		gateway::proto_network::HashSpacePush st(res.convert());
		{
			pthread_scoped_wrlock hslk(share->hs_rwlock());
			share->update_whs(st.wseed, hslk);
			share->update_rhs(st.rseed, hslk);
		}
	}
}


}  // namespace gateway
}  // namespace kumo

