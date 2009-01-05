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
		if(SESSION_IS_ACTIVE(from)) {
			shared_zone nullz;
			protocol::type::HashSpaceRequest arg;

			from->call(protocol::HashSpaceRequest, arg, nullz,
					BIND_RESPONSE(ResHashSpaceRequest), 10);
		}  // retry on Gateway::session_lost() if the node is lost
	} else {
		protocol::type::HashSpacePush st(res.convert());

		pthread_scoped_wrlock hslk(m_hs_rwlock);
		if(m_whs.empty() ||
				m_whs.clocktime() <= st.wseed().clocktime()) {
			m_whs = HashSpace(st.wseed());
		}
		if(m_rhs.empty() ||
				m_rhs.clocktime() <= st.rseed().clocktime()) {
			m_rhs = HashSpace(st.rseed());
		}
	}
}


RPC_FUNC(HashSpacePush, from, response, life, param)
try {
	LOG_DEBUG("HashSpacePush");

	pthread_scoped_wrlock hslk(m_hs_rwlock);
	if(m_whs.empty() ||
			m_whs.clocktime() <= param.wseed().clocktime()) {
		m_whs = HashSpace(param.wseed());
	}
	if(m_rhs.empty() ||
			m_rhs.clocktime() <= param.rseed().clocktime()) {
		m_rhs = HashSpace(param.rseed());
	}

	response.result(true);
}
RPC_CATCH(HashSpacePush, response)


}  // namespace kumo

