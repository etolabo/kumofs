#include "logic/srv_impl.h"

namespace kumo {


// COPY A-1
void Server::renew_w_hash_space()
{
	shared_zone nullz;
	protocol::type::WHashSpaceRequest arg;
	//              ^

	rpc::callback_t callback( BIND_RESPONSE(ResWHashSpaceRequest) );
	//                                         ^

	get_node(m_manager1)->call(
			protocol::WHashSpaceRequest, arg, nullz, callback, 10);
	//                ^

	if(m_manager2.connectable()) {
		get_node(m_manager2)->call(
				protocol::WHashSpaceRequest, arg, nullz, callback, 10);
	//                    ^
	}
}

// COPY A-2
void Server::renew_r_hash_space()
{
	shared_zone nullz;
	protocol::type::RHashSpaceRequest arg;
	//              ^

	rpc::callback_t callback( BIND_RESPONSE(ResRHashSpaceRequest) );
	//                                         ^

	get_node(m_manager1)->call(
			protocol::RHashSpaceRequest, arg, nullz, callback, 10);
	//                ^

	if(m_manager2.connectable()) {
		get_node(m_manager2)->call(
				protocol::RHashSpaceRequest, arg, nullz, callback, 10);
	//                    ^
	}
}


// COPY B-1
RPC_REPLY(ResWHashSpaceRequest, from, res, err, life)
{
	// FIXME is this function needed?
	if(!err.is_nil()) {
		LOG_DEBUG("WHashSpaceRequest failed ",err);
		if(SESSION_IS_ACTIVE(from)) {
			shared_zone nullz;
			protocol::type::WHashSpaceRequest arg;
			//              ^

			from->call(protocol::WHashSpaceRequest, arg, nullz,
			//                   ^
					BIND_RESPONSE(ResWHashSpaceRequest), 10);
					//               ^
		}  // retry on lost_node() if err.via.u64 == NODE_LOST?
	} else {
		LOG_DEBUG("renew hash space");
		HashSpace::Seed hsseed(res.convert());
		if(m_whs.empty() || m_whs.clocktime() < ClockTime(hsseed.clocktime())) {
		//   ^                ^
			m_whs = HashSpace(hsseed);
			//^
		}
	}
}

// COPY B-2
RPC_REPLY(ResRHashSpaceRequest, from, res, err, life)
{
	// FIXME is this function needed?
	if(!err.is_nil()) {
		LOG_DEBUG("WHashSpaceRequest failed ",err);
		if(SESSION_IS_ACTIVE(from)) {
			shared_zone nullz;
			protocol::type::RHashSpaceRequest arg;
			//              ^

			from->call(protocol::RHashSpaceRequest, arg, nullz,
			//                   ^
					BIND_RESPONSE(ResRHashSpaceRequest), 10);
					//               ^
		}  // retry on lost_node() if err.via.u64 == NODE_LOST?
	} else {
		LOG_DEBUG("renew hash space");
		HashSpace::Seed hsseed(res.convert());
		if(m_rhs.empty() || m_rhs.clocktime() < ClockTime(hsseed.clocktime())) {
		//   ^                ^
			m_rhs = HashSpace(hsseed);
			//^
		}
	}
}


CLUSTER_FUNC(HashSpaceSync, from, response, z, param)
try {
	LOG_DEBUG("HashSpaceSync");

	m_clock.update(param.clock());

	bool ret = false;

	if(!param.wseed().empty() && (m_whs.empty() ||
			m_whs.clocktime() <= ClockTime(param.wseed().clocktime()))) {
		m_whs = HashSpace(param.wseed());
		ret = true;
	}

	if(!param.rseed().empty() && (m_rhs.empty() ||
			m_rhs.clocktime() <= ClockTime(param.rseed().clocktime()))) {
		m_rhs = HashSpace(param.rseed());
		ret = true;
	}

	if(ret) {
		response.result(true);
	} else {
		response.null();
	}
}
RPC_CATCH(HashSpaceSync, response)



void Server::keep_alive()
{
	LOG_TRACE("keep alive ...");
	shared_zone nullz;
	protocol::type::KeepAlive arg(m_clock.get_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(ResKeepAlive) );

	get_node(m_manager1)->call(
			protocol::KeepAlive, arg, nullz, callback, 10);

	if(m_manager2.connectable()) {
		get_node(m_manager2)->call(
				protocol::KeepAlive, arg, nullz, callback, 10);
	}
}

RPC_REPLY(ResKeepAlive, from, res, err, life)
{
	if(err.is_nil()) {
		LOG_TRACE("KeepAlive succeeded");
	} else {
		LOG_DEBUG("KeepAlive failed: ",err);
	}
}


CLUSTER_FUNC(KeepAlive, from, response, z, param)
try {
	m_clock.update(param.clock());
	response.null();
}
RPC_CATCH(KeepAlive, response)


}  // namespace kumo

