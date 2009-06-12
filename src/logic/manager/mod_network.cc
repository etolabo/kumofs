#include "manager/framework.h"
#include "manager/mod_network.h"
#include "server/mod_network.h"
#include "gateway/mod_network.h"


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


RPC_IMPL(mod_network_t, KeepAlive, req, z, response)
{
	net->clock_update(req.param().adjust_clock);
	response.null();
}


RPC_IMPL(mod_network_t, HashSpaceRequest, req, z, response)
{
	HashSpace::Seed* wseed;
	HashSpace::Seed* rseed;
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		wseed = z->allocate<HashSpace::Seed>(share->whs());
		rseed = z->allocate<HashSpace::Seed>(share->rhs());
	}

	gateway::mod_network_t::HashSpacePush arg(*wseed, *rseed);
	response.result(arg, z);
}


RPC_IMPL(mod_network_t, WHashSpaceRequest, req, z, response)
{
	HashSpace::Seed* seed;
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		seed = z->allocate<HashSpace::Seed>(share->whs());
	}
	response.result(*seed, z);
}


RPC_IMPL(mod_network_t, RHashSpaceRequest, req, z, response)
{
	HashSpace::Seed* seed;
	{
		pthread_scoped_lock hslk(share->hs_mutex());
		seed = z->allocate<HashSpace::Seed>(share->rhs());
	}
	response.result(*seed, z);
}



void mod_network_t::sync_hash_space_servers(REQUIRE_HSLK)
{
	shared_zone life(new msgpack::zone());
	HashSpace::Seed* wseed = life->allocate<HashSpace::Seed>(share->whs());
	HashSpace::Seed* rseed = life->allocate<HashSpace::Seed>(share->rhs());

	server::mod_network_t::HashSpaceSync param(*wseed, *rseed, net->clock_incr());

	rpc::callback_t callback( BIND_RESPONSE(mod_network_t, HashSpaceSync) );

	net->for_each_node(ROLE_SERVER,
			for_each_call(param, life, callback, 10));
}


void mod_network_t::sync_hash_space_partner(REQUIRE_HSLK)
{
	if(!share->partner().connectable()) { return; }

	shared_zone life(new msgpack::zone());
	HashSpace::Seed* wseed = life->allocate<HashSpace::Seed>(share->whs());
	HashSpace::Seed* rseed = life->allocate<HashSpace::Seed>(share->rhs());

	manager::mod_network_t::HashSpaceSync param(*wseed, *rseed, net->clock_incr());
	net->get_node(share->partner())->call(
			param, life,
			BIND_RESPONSE(mod_network_t, HashSpaceSync), 10);
}

RPC_REPLY_IMPL(mod_network_t, HashSpaceSync, from, res, err, z)
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
			LOG_DEBUG("push hash space to ",(void*)p.get());
			p->call(param, life, callback, 10);
		}

	private:
		rpc::shared_zone& life;
		gateway::mod_network_t::HashSpacePush param;
		rpc::callback_t callback;
	};
}  // noname namespace

void mod_network_t::push_hash_space_clients(REQUIRE_HSLK)
try {
	LOG_DEBUG("push hash space ...");

	shared_zone life(new msgpack::zone());
	HashSpace::Seed* wseed = life->allocate<HashSpace::Seed>(share->whs());
	HashSpace::Seed* rseed = life->allocate<HashSpace::Seed>(share->rhs());

	rpc::callback_t callback( BIND_RESPONSE(mod_network_t, HashSpacePush) );
	net->subsystem().for_each_peer( each_client_push(wseed, rseed, callback, life) );

	// ignore error
} catch (std::runtime_error& e) {
	LOG_ERROR("HashSpacePush failed: ",e.what());
} catch (...) {
	LOG_ERROR("HashSpacePush failed: unknown error");
}

RPC_REPLY_IMPL(mod_network_t, HashSpacePush, from, res, err, z)
{ }



RPC_IMPL(mod_network_t, HashSpaceSync, req, z, response)
{
	if(req.node()->addr() != share->partner()) {
		throw std::runtime_error("unknown partner node");
	}

	net->clock_update(req.param().adjust_clock);

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


void mod_network_t::keep_alive()
{
	LOG_TRACE("keep alive ...");
	shared_zone nullz;
	server::mod_network_t::KeepAlive param(net->clock_incr());

	rpc::callback_t callback( BIND_RESPONSE(mod_network_t, KeepAlive) );

	// FIXME exception
	net->for_each_node(ROLE_SERVER,
			for_each_call(param, nullz, callback, 10));

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

RPC_REPLY_IMPL(mod_network_t, KeepAlive, from, res, err, z)
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

