//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#include "manager/framework.h"
#include "manager/mod_replace.h"
#include "server/mod_replace.h"

namespace kumo {
namespace manager {


mod_replace_t::mod_replace_t() :
	m_delayed_replace_cas(0)
{ }

mod_replace_t::~mod_replace_t() { }


void mod_replace_t::add_server(const address& addr, shared_node& s)
{
	LOG_INFO("server connected ",s->addr());
	TLOGPACK("nS",3,
			"addr", addr);

	bool change = false;

	pthread_scoped_lock hslk(share->hs_mutex());
	pthread_scoped_lock nslk(share->new_servers_mutex());

	if(!share->whs().server_is_active(addr)) {
		change = true;
		share->new_servers().push_back( weak_node(s) );
	}

	nslk.unlock();
	hslk.unlock();

	if(change && share->cfg_auto_replace()) {
		// delayed replace
		delayed_replace_election();
	}
}

void mod_replace_t::remove_server(const address& addr)
{
	LOG_WARN("server lost ",addr);
	TLOGPACK("lS",3,
			"addr", addr);

	ClockTime ct = net->clock_incr_clocktime();

	pthread_scoped_lock hslk(share->hs_mutex());
	pthread_scoped_lock nslk(share->new_servers_mutex());

	bool wfault = share->whs().fault_server(ct, addr);
	bool rfault = share->rhs().fault_server(ct, addr);

	if(wfault || rfault) {
		net->mod_network.sync_hash_space_partner(hslk);
		net->mod_network.sync_hash_space_servers(hslk);
		net->mod_network.push_hash_space_clients(hslk);
	}
	hslk.unlock();

	for(new_servers_t::iterator it(share->new_servers().begin());
			it != share->new_servers().end(); ) {
		shared_node n(it->lock());
		if(!n || n->addr() == addr) {
			it = share->new_servers().erase(it);
		} else {
			++it;
		}
	}
	nslk.unlock();

	if(share->cfg_auto_replace()) {
		// delayed replace
		delayed_replace_election();
	} else {
		pthread_scoped_lock relk(m_replace_mutex);
		m_copying.invalidate();  // prevent replace delete
	}
}


void mod_replace_t::delayed_replace_election()
{
	int cas = __sync_add_and_fetch(&m_delayed_replace_cas, 1);
	net->do_after(
			share->cfg_replace_delay_seconds() * framework::DO_AFTER_BY_SECONDS,
			mp::bind(&mod_replace_t::cas_checked_replace_election, this, cas));
	LOG_INFO("set delayed replace after ",share->cfg_replace_delay_seconds()," seconds");
}

void mod_replace_t::cas_checked_replace_election(int cas)
{
	if(m_delayed_replace_cas == cas) {
		replace_election();
	}
}


void mod_replace_t::replace_election()
{
	// XXX
	// election: smaller address has priority
	pthread_scoped_lock hslk(share->hs_mutex());
	attach_new_servers(hslk);
	detach_fault_servers(hslk);

	if(share->partner().connectable() && share->partner() < net->addr()) {
		LOG_INFO("replace delegate to ",share->partner());
	
		// delegate replace
		shared_zone life(new msgpack::zone());

		HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(share->whs());
		hslk.unlock();

		manager::mod_replace_t::ReplaceElection param(*seed, net->clock_incr());
		net->get_node(share->partner())->call(  // FIXME exception
				param, life,
				BIND_RESPONSE(mod_replace_t, ReplaceElection), 10);
	} else {
		LOG_INFO("replace self elected");
		start_replace(hslk);
	}
}

RPC_REPLY_IMPL(mod_replace_t, ReplaceElection, from, res, err, z)
{
	if(!err.is_nil() || res.is_nil()) {
		LOG_WARN("replace delegate failed, elected");
		pthread_scoped_lock hslk(share->hs_mutex());
		start_replace(hslk);
	} else {
		// do nothing
	}
}



void mod_replace_t::attach_new_servers(REQUIRE_HSLK)
{
	// update hash space
	ClockTime ct = net->clock_incr_clocktime();
	LOG_INFO("update hash space at time(",ct.get(),")");

	pthread_scoped_lock nslk(share->new_servers_mutex());

	for(new_servers_t::iterator it(share->new_servers().begin()),
			it_end(share->new_servers().end()); it != it_end; ++it) {
		shared_node srv(it->lock());
		if(srv) {
			if(share->whs().server_is_include(srv->addr())) {
				LOG_INFO("recover server: ",srv->addr());
				share->whs().recover_server(ct, srv->addr());
			} else {
				LOG_INFO("new server: ",srv->addr());
				share->whs().add_server(ct, srv->addr());
			}
		}
	}
	share->new_servers().clear();

	nslk.unlock();

	net->mod_network.sync_hash_space_partner(hslk);
	//net->mod_network.sync_hash_space_servers(hslk);
	//net->mod_network.push_hash_space_clients(hslk);
}

void mod_replace_t::detach_fault_servers(REQUIRE_HSLK)
{
	ClockTime ct = net->clock_incr_clocktime();

	share->whs().remove_fault_servers(ct);

	net->mod_network.sync_hash_space_partner(hslk);
	//net->mod_network.sync_hash_space_servers(hslk);
	//net->mod_network.push_hash_space_clients(hslk);
}


mod_replace_t::progress::progress() :
	m_clocktime(0) { }

mod_replace_t::progress::~progress() { }

inline ClockTime mod_replace_t::progress::clocktime() const
{
	return m_clocktime;
}

inline void mod_replace_t::progress::reset(ClockTime replace_time, const nodes_t& nodes)
{
	m_target_nodes = m_remainder = nodes;
	m_clocktime = replace_time;
}

bool mod_replace_t::progress::pop(ClockTime replace_time, const rpc::address& node)
{
	if(m_clocktime != replace_time) { return false; }
	if(m_remainder.empty()) { return false; }

	nodes_t::iterator erase_from =
		std::remove(m_remainder.begin(), m_remainder.end(), node);
	m_remainder.erase(erase_from, m_remainder.end());

	return m_remainder.empty();
}

mod_replace_t::progress::nodes_t mod_replace_t::progress::invalidate()
{
	m_clocktime = ClockTime(0);
	m_remainder.clear();

	nodes_t tmp;
	m_target_nodes.swap(tmp);
	return tmp;
}


namespace {
	template <typename nodes_t>
	struct gather_address {
		gather_address(nodes_t& target_nodes) :
			m_target_nodes(target_nodes) { }
		void operator() (shared_node& n)
		{
			m_target_nodes.push_back(n->addr());
		}
	private:
		nodes_t& m_target_nodes;
	};
}  // noname namespace

void mod_replace_t::start_replace(REQUIRE_HSLK, bool full)
{
	LOG_INFO("start replace copy; full=",full);
	pthread_scoped_lock relk(m_replace_mutex);

	shared_zone life(new msgpack::zone());

	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(share->whs());
	ClockTime replace_time(share->whs().clocktime());

	server::mod_replace_t::ReplaceCopyStart param(*seed, net->clock_incr(), full);

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(mod_replace_t, ReplaceCopyStart) );

	progress::nodes_t target_nodes;
	net->for_each_node(ROLE_SERVER,
			for_each_call_do(param, life, callback, 10,
			gather_address<progress::nodes_t>(target_nodes)));

	LOG_INFO("active node: ",target_nodes.size());
	m_copying.reset(replace_time, target_nodes);
	m_deleting.invalidate();
	relk.unlock();

	// push hashspace to the clients
	net->mod_network.push_hash_space_clients(hslk);
}

RPC_REPLY_IMPL(mod_replace_t, ReplaceCopyStart, from, res, err, z)
{
	// FIXME
}


RPC_IMPL(mod_replace_t, ReplaceElection, req, z, response)
{
	LOG_DEBUG("ReplaceElection");

	if(req.node()->addr() != share->partner()) {
		throw std::runtime_error("unknown partner node");
	}

	net->clock_update(req.param().adjust_clock);

	pthread_scoped_lock hslk(share->hs_mutex());

	if(req.param().hsseed.empty() ||
			ClockTime(req.param().hsseed.clocktime()) < share->whs().clocktime()) {
		LOG_DEBUG("obsolete hashspace");
		response.result(true);
		return;
	}

	if(share->whs().clocktime() < req.param().hsseed.clocktime() ||
			share->whs() == req.param().hsseed) {
		LOG_INFO("double replace guard ",share->partner());

	} else {
		// election: smaller address has priority
		if(share->partner() < net->addr()) {
			LOG_INFO("replace re-delegate to ",share->partner());
			response.null();
		} else {
			LOG_INFO("replace delegated from ",share->partner());
			attach_new_servers(hslk);
			detach_fault_servers(hslk);
			start_replace(hslk);
			hslk.unlock();
			response.result(true);
		}
	}
}



RPC_IMPL(mod_replace_t, ReplaceCopyEnd, req, z, response)
{
	net->clock_update(req.param().adjust_clock);

	{
		pthread_scoped_lock relk(m_replace_mutex);

		ClockTime replace_time(req.param().replace_time);
		if(m_copying.pop(replace_time, req.node()->addr())) {
			finish_replace_copy(relk);
		}
	}

	response.result(true);
}


RPC_IMPL(mod_replace_t, ReplaceDeleteEnd, req, z, response)
{
	net->clock_update(req.param().adjust_clock);

	{
		pthread_scoped_lock relk(m_replace_mutex);

		ClockTime replace_time(req.param().replace_time);
		if(m_deleting.pop(replace_time, req.node()->addr())) {
			finish_replace(relk);
		}
	}

	response.result(true);
}


void mod_replace_t::finish_replace_copy(REQUIRE_RELK)
{
	ClockTime replace_time = m_copying.clocktime();
	LOG_INFO("start replace delete time(",replace_time.get(),")");

	progress::nodes_t target_nodes = m_copying.invalidate();

	shared_zone life(new msgpack::zone());
	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(share->whs());
	// FIXME server::mod_replace_t::ReplaceDeleteStart has HashSpace::Seed:
	//       not so good efficiency
	server::mod_replace_t::ReplaceDeleteStart param(*seed, net->clock_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(mod_replace_t, ReplaceDeleteStart) );

	for(progress::nodes_t::iterator it(target_nodes.begin()),
			it_end(target_nodes.end()); it != it_end; ++it) {
		net->get_node(*it)->call(param, life, callback, 10);
	}

	m_deleting.reset(replace_time, target_nodes);

	pthread_scoped_lock hslk(share->hs_mutex());
	share->rhs() = share->whs();

	net->mod_network.push_hash_space_clients(hslk);
	//net->mod_network.sync_hash_space_servers(hslk);
	net->mod_network.sync_hash_space_partner(hslk);
}

RPC_REPLY_IMPL(mod_replace_t, ReplaceDeleteStart, from, res, err, z)
{
	// FIXME
}


inline void mod_replace_t::finish_replace(REQUIRE_RELK)
{
	LOG_INFO("replace finished time(",m_deleting.clocktime().get(),")");
	m_deleting.invalidate();
}


}  // namespace manager
}  // namespace kumo

