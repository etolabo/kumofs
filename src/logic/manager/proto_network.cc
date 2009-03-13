#include "manager/framework.h"
#include "manager/proto_network.h"
#include "server/proto_network.h"
#include "gateway/proto_network.h"


#define EACH_ACTIVE_NEW_COMERS_BEGIN(NODE) \
	for(new_servers_t::iterator _it_(share->new_servers().begin()), \
			it_end(share->new_servers().end()); _it_ != it_end; ++_it_) { \
		shared_node NODE(_it_->lock()); \
		if(SESSION_IS_ACTIVE(NODE)) {
			// FIXME share->new_servers().erase(it) ?

#define EACH_ACTIVE_NEW_COMERS_END \
		} \
	}

namespace kumo {
namespace manager {


RPC_IMPL(proto_network, KeepAlive_1, req, z, response)
try {
	net->clock_update(req.param().clock);
	response.null();
}
RPC_CATCH(KeepAlive_1, response)


RPC_IMPL(proto_network, HashSpaceRequest_1, req, z, response)
try {
	HashSpace::Seed* wseed;
	HashSpace::Seed* rseed;
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		wseed = z->allocate<HashSpace::Seed>(share->whs());
		rseed = z->allocate<HashSpace::Seed>(share->rhs());
	}

	gateway::proto_network::HashSpacePush_1 arg(*wseed, *rseed);
	response.result(arg, z);
}
RPC_CATCH(HashSpaceRequest_1, response)


RPC_IMPL(proto_network, WHashSpaceRequest_1, req, z, response)
try {
	HashSpace::Seed* seed;
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		seed = z->allocate<HashSpace::Seed>(share->whs());
	}
	response.result(*seed, z);
}
RPC_CATCH(WHashSpaceRequest_1, response)


RPC_IMPL(proto_network, RHashSpaceRequest_1, req, z, response)
try {
	HashSpace::Seed* seed;
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		seed = z->allocate<HashSpace::Seed>(share->rhs());
	}
	response.result(*seed, z);
}
RPC_CATCH(RHashSpaceRequest_1, response)



void proto_network::sync_hash_space_servers(REQUIRE_HSLK, REQUIRE_SSLK)
{
	shared_zone life(new msgpack::zone());
	HashSpace::Seed* wseed = life->allocate<HashSpace::Seed>(share->whs());
	HashSpace::Seed* rseed = life->allocate<HashSpace::Seed>(share->rhs());

	server::proto_network::HashSpaceSync_1 param(*wseed, *rseed, net->clock_incr());

	rpc::callback_t callback( BIND_RESPONSE(proto_network, HashSpaceSync_1) );

	EACH_ACTIVE_SERVERS_BEGIN(node)
		node->call(param, life, callback, 10);
	EACH_ACTIVE_SERVERS_END
}


void proto_network::sync_hash_space_partner(REQUIRE_HSLK)
{
	if(!share->partner().connectable()) { return; }

	shared_zone life(new msgpack::zone());
	HashSpace::Seed* wseed = life->allocate<HashSpace::Seed>(share->whs());
	HashSpace::Seed* rseed = life->allocate<HashSpace::Seed>(share->rhs());

	manager::proto_network::HashSpaceSync_1 param(*wseed, *rseed, net->clock_incr());
	net->get_node(share->partner())->call(
			param, life,
			BIND_RESPONSE(proto_network, HashSpaceSync_1), 10);
}

RPC_REPLY_IMPL(proto_network, HashSpaceSync_1, from, res, err, life)
{
	// FIXME retry
}


namespace {
	struct each_client_push {
		each_client_push(HashSpace::Seed* whs, HashSpace::Seed* rhs,
				rpc::callback_t cb, shared_zone& l) :
			life(l),
			param(*whs, *rhs),
			callback(cb) { }

		void operator() (rpc::shared_peer p)
		{
			LOG_WARN("push hash space to ",(void*)p.get());
			p->call(param, life, callback, 10);
		}

	private:
		rpc::shared_zone& life;
		gateway::proto_network::HashSpacePush_1 param;
		rpc::callback_t callback;
	};
}  // noname namespace

void proto_network::push_hash_space_clients(REQUIRE_HSLK)
{
	LOG_WARN("push hash space ...");

	shared_zone life(new msgpack::zone());
	HashSpace::Seed* wseed = life->allocate<HashSpace::Seed>(share->whs());
	HashSpace::Seed* rseed = life->allocate<HashSpace::Seed>(share->rhs());

	rpc::callback_t callback( BIND_RESPONSE(proto_network, HashSpacePush_1) );
	net->subsystem().for_each_peer( each_client_push(wseed, rseed, callback, life) );
}

RPC_REPLY_IMPL(proto_network, HashSpacePush_1, from, res, err, life)
{ }



RPC_IMPL(proto_network, HashSpaceSync_1, req, z, response)
try {
	if(req.node()->addr() != share->partner()) {
		throw std::runtime_error("unknown partner node");
	}

	net->clock_update(req.param().clock);

	bool ret = false;

	pthread_scoped_lock hslk(share->hs_mutex());
	pthread_scoped_lock nslk(share->new_servers_mutex());

	if(!req.param().wseed.empty() && (share->whs().empty() ||
			share->whs().clocktime() <= ClockTime(req.param().wseed.clocktime()))) {
		share->whs() = HashSpace(req.param().wseed);
		ret = true;
	}

	if(!req.param().rseed.empty() && (share->rhs().empty() ||
			share->rhs().clocktime() <= ClockTime(req.param().rseed.clocktime()))) {
		share->rhs() = HashSpace(req.param().rseed);
		ret = true;
	}

	for(new_servers_t::iterator it(share->new_servers().begin());
			it != share->new_servers().end(); ) {
		shared_node srv(it->lock());
		if(!srv || share->whs().server_is_active(srv->addr())) {
			it = share->new_servers().erase(it);
		} else {
			++it;
		}
	}

	nslk.unlock();
	hslk.unlock();

	if(ret) {
		response.result(true);
	} else {
		response.null();
	}
}
RPC_CATCH(HashSpaceSync_1, response)


void proto_network::keep_alive()
{
	LOG_TRACE("keep alive ...");
	shared_zone nullz;
	server::proto_network::KeepAlive_1 param(net->clock_incr());

	rpc::callback_t callback( BIND_RESPONSE(proto_network, KeepAlive_1) );

	pthread_scoped_lock sslk(share->servers_mutex());
	EACH_ACTIVE_SERVERS_BEGIN(node)
		// FIXME exception
		node->call(param, nullz, callback, 10);
	EACH_ACTIVE_SERVERS_END
	sslk.unlock();

	pthread_scoped_lock nslk(share->new_servers_mutex());
	EACH_ACTIVE_NEW_COMERS_BEGIN(node)
		// FIXME exception
		node->call(param, nullz, callback, 10);
	EACH_ACTIVE_NEW_COMERS_END
	nslk.unlock();

	if(share->partner().connectable()) {
		// FIXME cache result of net->get_node(share->partner())?
		net->get_node(share->partner())->call(
				param, nullz, callback, 10);
	}
}

RPC_REPLY_IMPL(proto_network, KeepAlive_1, from, res, err, life)
{
	if(err.is_nil()) {
		LOG_TRACE("KeepAlive succeeded");
	} else {
		LOG_WARN("KeepAlive failed: ",err);
		if(from && !from->is_lost()) {
			if(from->increment_connect_retried_count() > 5) {  // FIXME
				from->shutdown();
			}
		}
	}
}


}  // namespace manager
}  // namespace kumo

