#include "gateway/framework.h"
#include "gateway/proto_network.h"
#include "manager/proto_network.h"

namespace kumo {
namespace gateway {


RPC_IMPL(proto_network, HashSpacePush_1, req, z, response)
try {
	LOG_DEBUG("HashSpacePush");

	pthread_scoped_wrlock hslk(share->hs_rwlock());

	if(share->whs().empty() ||
			share->whs().clocktime() <= req.param().wseed.clocktime()) {
		share->whs() = HashSpace(req.param().wseed);
	}

	if(share->rhs().empty() ||
			share->rhs().clocktime() <= req.param().rseed.clocktime()) {
		share->rhs() = HashSpace(req.param().rseed);
	}

	response.result(true);
}
RPC_CATCH(HashSpacePush, response)


void proto_network::renew_hash_space()
{
	shared_zone nullz;
	manager::proto_network::HashSpaceRequest_1 param;

	rpc::callback_t callback( BIND_RESPONSE(proto_network, HashSpaceRequest_1) );

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
	manager::proto_network::HashSpaceRequest_1 param;
	ns->call(param, nullz,
			BIND_RESPONSE(proto_network, HashSpaceRequest_1), 10);
}

RPC_REPLY_IMPL(proto_network, HashSpaceRequest_1, from, res, err, life)
{
	if(!err.is_nil()) {
		LOG_DEBUG("HashSpaceRequest failed ",err);
		if(SESSION_IS_ACTIVE(from)) {
			shared_zone nullz;
			manager::proto_network::HashSpaceRequest_1 param;

			from->call(param, nullz,
					BIND_RESPONSE(proto_network, HashSpaceRequest_1), 10);
		}  // retry on Gateway::session_lost() if the node is lost
	} else {
		gateway::proto_network::HashSpacePush_1 st(res.convert());

		pthread_scoped_wrlock hslk(share->hs_rwlock());
		if(share->whs().empty() ||
				share->whs().clocktime() <= st.wseed.clocktime()) {
			share->whs() = HashSpace(st.wseed);
		}
		if(share->rhs().empty() ||
				share->rhs().clocktime() <= st.rseed.clocktime()) {
			share->rhs() = HashSpace(st.rseed);
		}
	}
}


}  // namespace gateway
}  // namespace kumo

