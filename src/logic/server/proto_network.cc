#include "server/framework.h"
#include "server/proto_network.h"
#include "manager/proto_network.h"

namespace kumo {
namespace server {


RPC_IMPL(proto_network, KeepAlive_1, req, z, response)
{
	net->clock_update(req.param().clock);
	response.null();
}


void proto_network::keep_alive()
{
	LOG_TRACE("keep alive ...");
	shared_zone nullz;
	manager::proto_network::KeepAlive_1 param(net->clock_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(proto_network, KeepAlive_1) );

	net->get_node(share->manager1())->call(
			param, nullz, callback, 10);

	if(share->manager2().connectable()) {
		net->get_node(share->manager2())->call(
				param, nullz, callback, 10);
	}
}

RPC_REPLY_IMPL(proto_network, KeepAlive_1, from, res, err, life)
{
	if(err.is_nil()) {
		LOG_TRACE("KeepAlive succeeded");
	} else {
		LOG_DEBUG("KeepAlive failed: ",err);
	}
}



RPC_IMPL(proto_network, HashSpaceSync_1, req, z, response)
{
	LOG_DEBUG("HashSpaceSync_1");

	net->clock_update(req.param().clock);

	bool ret = false;

	pthread_scoped_wrlock whlk(share->whs_mutex());

	if(share->whs().clocktime() <= ClockTime(req.param().wseed.clocktime())) {
		share->whs() = HashSpace(req.param().wseed);
		ret = true;
	}

	pthread_scoped_wrlock rhlk(share->rhs_mutex());

	if(share->rhs().clocktime() <= ClockTime(req.param().rseed.clocktime())) {
		share->rhs() = HashSpace(req.param().rseed);
		ret = true;
	}

	rhlk.unlock();
	whlk.unlock();

	if(ret) {
		response.result(true);
	} else {
		response.null();
	}
}



// FIXME needed?: renew_w_hash_space, renew_r_hash_space
// COPY A-1
void proto_network::renew_w_hash_space()
{
	shared_zone nullz;
	manager::proto_network::WHashSpaceRequest_1 param;
	//              ^

	rpc::callback_t callback( BIND_RESPONSE(proto_network, WHashSpaceRequest_1) );
	//                                         ^

	net->get_node(share->manager1())->call(
			param, nullz, callback, 10);
	//                ^

	if(share->manager2().connectable()) {
		net->get_node(share->manager2())->call(
				param, nullz, callback, 10);
	//                    ^
	}
}

// COPY A-2
void proto_network::renew_r_hash_space()
{
	shared_zone nullz;
	manager::proto_network::RHashSpaceRequest_1 param;
	//              ^

	rpc::callback_t callback( BIND_RESPONSE(proto_network, RHashSpaceRequest_1) );
	//                                         ^

	net->get_node(share->manager1())->call(
			param, nullz, callback, 10);
	//                ^

	if(share->manager2().connectable()) {
		net->get_node(share->manager2())->call(
				param, nullz, callback, 10);
	//                    ^
	}
}


// COPY B-1
RPC_REPLY_IMPL(proto_network, WHashSpaceRequest_1, from, res, err, life)
{
	// FIXME is this function needed?
	if(!err.is_nil()) {
		LOG_DEBUG("WHashSpaceRequest failed ",err);
		if(SESSION_IS_ACTIVE(from)) {
			shared_zone nullz;
			manager::proto_network::WHashSpaceRequest_1 param;
			//              ^

			from->call(param, nullz,
			//                   ^
					BIND_RESPONSE(proto_network, WHashSpaceRequest_1), 10);
					//               ^
		}  // retry on lost_node() if err.via.u64 == NODE_LOST?
	} else {
		LOG_DEBUG("renew hash space");
		HashSpace::Seed hsseed(res.convert());

		pthread_scoped_wrlock whlk(share->whs_mutex());
		if(share->whs().empty() || share->whs().clocktime() < ClockTime(hsseed.clocktime())) {
		//   ^                ^
			share->whs() = HashSpace(hsseed);
			//^
		}
	}
}

// COPY B-2
RPC_REPLY_IMPL(proto_network, RHashSpaceRequest_1, from, res, err, life)
{
	// FIXME is this function needed?
	if(!err.is_nil()) {
		LOG_DEBUG("WHashSpaceRequest failed ",err);
		if(SESSION_IS_ACTIVE(from)) {
			shared_zone nullz;
			manager::proto_network::RHashSpaceRequest_1 param;
			//              ^

			from->call(param, nullz,
			//                   ^
					BIND_RESPONSE(proto_network, RHashSpaceRequest_1), 10);
					//               ^
		}  // retry on lost_node() if err.via.u64 == NODE_LOST?
	} else {
		LOG_DEBUG("renew hash space");
		HashSpace::Seed hsseed(res.convert());

		pthread_scoped_wrlock rhlk(share->rhs_mutex());
		if(share->rhs().empty() || share->rhs().clocktime() < ClockTime(hsseed.clocktime())) {
		//   ^                ^
			share->rhs() = HashSpace(hsseed);
			//^
		}
	}
}


}  // namespace server
}  // namespace kumo

