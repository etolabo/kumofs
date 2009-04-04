#include "server/framework.h"
#include "server/mod_network.h"
#include "manager/mod_network.h"

namespace kumo {
namespace server {


RPC_IMPL(mod_network_t, KeepAlive, req, z, response)
{
	net->clock_update(req.param().adjust_clock);
	response.null();
}


void mod_network_t::keep_alive()
{
	LOG_TRACE("keep alive ...");
	shared_zone nullz;
	manager::mod_network_t::KeepAlive param(net->clock_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(mod_network_t, KeepAlive) );

	net->get_node(share->manager1())->call(
			param, nullz, callback, 10);

	if(share->manager2().connectable()) {
		net->get_node(share->manager2())->call(
				param, nullz, callback, 10);
	}
}

RPC_REPLY_IMPL(mod_network_t, KeepAlive, from, res, err, z)
{
	if(err.is_nil()) {
		LOG_TRACE("KeepAlive succeeded");
	} else {
		LOG_DEBUG("KeepAlive failed: ",err);
	}
}



RPC_IMPL(mod_network_t, HashSpaceSync, req, z, response)
{
	LOG_DEBUG("HashSpaceSync");

	net->clock_update(req.param().adjust_clock);

	bool ret = false;

	pthread_scoped_wrlock whlk(share->whs_mutex());
//	typedef std::vector<HashSpace::node> nodes_t;

	if(share->whs().clocktime() <= req.param().wseed.clocktime() &&
			!req.param().wseed.empty()) {
		share->whs() = HashSpace(req.param().wseed);
		ret = true;
	}
//	if(share->whs().clocktime() <= req.param().wseed.clocktime() &&
//			!req.param().wseed.empty()) {
//		for(nodes_t::const_iterator it(req.param().wseed.nodes().begin()),
//				it_end(req.param().wseed.nodes().end()); it != it_end; ++it) {
//			if(!it->is_active()) {
//LOG_ERROR("whs fault: ",it->addr());
//				if(share->whs().fault_server(req.param().wseed.clocktime(), it->addr())) {
//					ret = true;
//				}
//			}
//		}
//	}

	pthread_scoped_wrlock rhlk(share->rhs_mutex());

	if(share->rhs().clocktime() <= req.param().rseed.clocktime() &&
			!req.param().rseed.empty()) {
		share->rhs() = HashSpace(req.param().rseed);
		ret = true;
	}
//	if(share->rhs().clocktime() <= req.param().rseed.clocktime() &&
//			!req.param().rseed.empty()) {
//		for(nodes_t::const_iterator it(req.param().rseed.nodes().begin()),
//				it_end(req.param().rseed.nodes().end()); it != it_end; ++it) {
//			if(!it->is_active()) {
//LOG_ERROR("rhs fault: ",it->addr());
//				if(share->rhs().fault_server(req.param().rseed.clocktime(), it->addr())) {
//					ret = true;
//				}
//			}
//		}
//	}

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
void mod_network_t::renew_w_hash_space()
{
	shared_zone nullz;
	manager::mod_network_t::WHashSpaceRequest param;
	//              ^

	rpc::callback_t callback( BIND_RESPONSE(mod_network_t, WHashSpaceRequest) );
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
void mod_network_t::renew_r_hash_space()
{
	shared_zone nullz;
	manager::mod_network_t::RHashSpaceRequest param;
	//              ^

	rpc::callback_t callback( BIND_RESPONSE(mod_network_t, RHashSpaceRequest) );
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
RPC_REPLY_IMPL(mod_network_t, WHashSpaceRequest, from, res, err, z)
{
	// FIXME is this function needed?
	if(!err.is_nil()) {
		LOG_DEBUG("WHashSpaceRequest failed ",err);
		if(SESSION_IS_ACTIVE(from)) {
			shared_zone nullz;
			manager::mod_network_t::WHashSpaceRequest param;
			//              ^

			from->call(param, nullz,
			//                   ^
					BIND_RESPONSE(mod_network_t, WHashSpaceRequest), 10);
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
RPC_REPLY_IMPL(mod_network_t, RHashSpaceRequest, from, res, err, z)
{
	// FIXME is this function needed?
	if(!err.is_nil()) {
		LOG_DEBUG("WHashSpaceRequest failed ",err);
		if(SESSION_IS_ACTIVE(from)) {
			shared_zone nullz;
			manager::mod_network_t::RHashSpaceRequest param;
			//              ^

			from->call(param, nullz,
			//                   ^
					BIND_RESPONSE(mod_network_t, RHashSpaceRequest), 10);
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

