#include "logic/gw_impl.h"

namespace kumo {


void Gateway::renew_hash_space()
{
	shared_zone nullz;
	protocol::type::HashSpaceRequest arg;

	rpc::callback_t callback( BIND_RESPONSE(ResHashSpaceRequest) );

	get_server(m_manager1)->call(
			protocol::HashSpaceRequest, arg, nullz, callback, 10);

	if(m_manager2.connectable()) {
		get_server(m_manager2)->call(
				protocol::HashSpaceRequest, arg, nullz, callback, 10);
	}
}

void Gateway::renew_hash_space_for(const address& addr)
{
	shared_session ns(get_server(addr));
	shared_zone nullz;
	protocol::type::HashSpaceRequest arg;
	ns->call(protocol::HashSpaceRequest, arg, nullz,
			BIND_RESPONSE(ResHashSpaceRequest), 10);
}

RPC_REPLY(ResHashSpaceRequest, from, res, err, life)
{
	if(!err.is_nil()) {
		LOG_DEBUG("HashSpaceRequest failed ",err);
		if(from && !from->is_lost()) {
			shared_zone nullz;
			protocol::type::HashSpaceRequest arg;

			from->call(protocol::HashSpaceRequest, arg, nullz,
					BIND_RESPONSE(ResHashSpaceRequest), 10);
		}  // retry on Gateway::session_lost() if the node is lost
	} else {
		if(m_hs.empty()) {
			LOG_DEBUG("renew hash space");
			HashSpace::Seed hsseed(res.convert());
			m_hs = HashSpace(hsseed);
		} else {
			HashSpace::Seed hsseed(res.convert());
			if(m_hs.clocktime() <= hsseed.clocktime()) {
				LOG_DEBUG("renew hash space");
				m_hs = HashSpace(hsseed);
			}
		}
	}
}


RPC_FUNC(HashSpacePush, from, response, life, param)
try {
	LOG_DEBUG("HashSpacePush");

	if(m_hs.empty() || m_hs.clocktime() < param.hsseed().clocktime()) {
		m_hs = HashSpace(param.hsseed());
	}

	response.result(true);
}
RPC_CATCH(HashSpacePush, response)


}  // namespace kumo

