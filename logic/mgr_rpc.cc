#include "logic/mgr_impl.h"

namespace kumo {


CLUSTER_FUNC(WHashSpaceRequest, from, response, life, param)
try {
	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(m_whs);
	response.result(*seed, life);
}
RPC_CATCH(WHashSpaceRequest, response)

CLUSTER_FUNC(RHashSpaceRequest, from, response, life, param)
try {
	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(m_rhs);
	response.result(*seed, life);
}
RPC_CATCH(RHashSpaceRequest, response)


CLISRV_FUNC(HashSpaceRequest, from, response, life, param)
try {
	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(m_whs);
	response.result(*seed, life);
}
RPC_CATCH(HashSpaceRequest, response)



//void Manager::push_hash_space()
//{
//	shared_zone life(new mp::zone());
//	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(m_whs);
//	// FIXME protocol::type::HashSpacePush has HashSpace::Seed:
//	//       not so good efficiency
//	protocol::type::HashSpacePush arg(*seed);
//
//	EACH_ACTIVE_SERVERS_BEGIN(node)
//		node->call(protocol::HashSpacePush, arg, life, callback, 10);
//	EACH_ACTIVE_SERVERS_END
//}


namespace {
	struct each_client_push {
		each_client_push(HashSpace& hs, rpc::callback_t cb) :
			life(new mp::zone()),
			arg( *life->allocate<HashSpace::Seed>(hs) ),
			callback(cb) { }

		void operator() (rpc::shared_peer p)
		{
			LOG_WARN("push hash space");
			p->call(protocol::HashSpacePush, arg, life, callback, 10);
		}

	private:
		rpc::shared_zone life;
		protocol::type::HashSpacePush arg;
		rpc::callback_t callback;
	};
}  // noname namespace

void Manager::push_hash_space_clients()
{
	LOG_WARN("push hash space ...");
	rpc::callback_t callback( BIND_RESPONSE(ResHashSpacePush) );
	m_clisrv.for_each_peer( each_client_push(m_whs, callback) );
}

RPC_REPLY(ResHashSpacePush, from, res, err, life)
{
	// FIXME retry
}


void Manager::sync_hash_space()
{
	shared_zone life(new mp::zone());
	HashSpace::Seed* wseed = life->allocate<HashSpace::Seed>(m_whs);
	HashSpace::Seed* rseed = life->allocate<HashSpace::Seed>(m_rhs);
	protocol::type::HashSpaceSync arg(*wseed, *rseed, m_clock.get_incr());
	get_node(m_partner)->call(
			protocol::HashSpaceSync, arg, life,
			BIND_RESPONSE(ResHashSpaceSync), 10);
}

RPC_REPLY(ResHashSpaceSync, from, res, err, life)
{
	// FIXME retry
}

CLUSTER_FUNC(HashSpaceSync, from, response, life, param)
try {
	if(from->addr() != m_partner) {
		throw std::runtime_error("unknown partner node");
	}

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


void Manager::keep_alive()
{
	LOG_TRACE("keep alive ...");
	shared_zone nullz;
	protocol::type::KeepAlive arg(m_clock.get_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(ResKeepAlive) );

	EACH_ACTIVE_SERVERS_BEGIN(node)
		// FIXME exception
		node->call(protocol::KeepAlive, arg, nullz, callback, 10);
	EACH_ACTIVE_SERVERS_END

	EACH_ACTIVE_NEW_COMERS_BEGIN(node)
		// FIXME exception
		node->call(protocol::KeepAlive, arg, nullz, callback, 10);
	EACH_ACTIVE_NEW_COMERS_END

	if(m_partner.connectable()) {
		// FIXME cache result of get_node(m_partner)
		get_node(m_partner)->call(
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


CLUSTER_FUNC(KeepAlive, from, response, life, param)
try {
	response.null();
}
RPC_CATCH(KeepAlive, response)


}  // namespace kumo

