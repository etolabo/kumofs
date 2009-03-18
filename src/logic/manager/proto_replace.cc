#include "manager/framework.h"
#include "manager/proto_replace.h"
#include "server/proto_replace.h"

namespace kumo {
namespace manager {


proto_replace::proto_replace() :
	m_delayed_replace_cas(0)
{ }

proto_replace::~proto_replace() { }


void proto_replace::add_server(const address& addr, shared_node& s)
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

void proto_replace::remove_server(const address& addr)
{
	LOG_INFO("server lost ",addr);
	TLOGPACK("lS",3,
			"addr", addr);

	ClockTime ct = net->clock_incr_clocktime();

	pthread_scoped_lock hslk(share->hs_mutex());
	pthread_scoped_lock sslk(share->servers_mutex());
	pthread_scoped_lock nslk(share->new_servers_mutex());

	bool wfault = share->whs().fault_server(ct, addr);
	bool rfault = share->rhs().fault_server(ct, addr);

	if(wfault || rfault) {
		net->scope_proto_network().sync_hash_space_partner(hslk);
		net->scope_proto_network().sync_hash_space_servers(hslk, sslk);
		net->scope_proto_network().push_hash_space_clients(hslk);
	}
	hslk.unlock();

	share->servers().erase(addr);
	sslk.unlock();

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


void proto_replace::delayed_replace_election()
{
	int cas = __sync_add_and_fetch(&m_delayed_replace_cas, 1);
	net->do_after(
			share->cfg_replace_delay_seconds() * framework::DO_AFTER_BY_SECONDS,
			mp::bind(&proto_replace::cas_checked_replace_election, this, cas));
	LOG_INFO("set delayed replace after ",share->cfg_replace_delay_seconds()," seconds");
}

void proto_replace::cas_checked_replace_election(int cas)
{
	if(m_delayed_replace_cas == cas) {
		replace_election();
	}
}


void proto_replace::replace_election()
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

		manager::proto_replace::ReplaceElection_1 param(*seed, net->clock_incr());
		net->get_node(share->partner())->call(  // FIXME exception
				param, life,
				BIND_RESPONSE(proto_replace, ReplaceElection_1), 10);
	} else {
		LOG_INFO("replace self elected");
		start_replace(hslk);
	}
}

RPC_REPLY_IMPL(proto_replace, ReplaceElection_1, from, res, err, life)
{
	if(!err.is_nil() || res.is_nil()) {
		LOG_INFO("replace delegate failed, elected");
		pthread_scoped_lock hslk(share->hs_mutex());
		start_replace(hslk);
	} else {
		// do nothing
	}
}



void proto_replace::attach_new_servers(REQUIRE_HSLK)
{
	// update hash space
	ClockTime ct = net->clock_incr_clocktime();
	LOG_INFO("update hash space at time(",ct.get(),")");

	pthread_scoped_lock nslk(share->new_servers_mutex());
	pthread_scoped_lock sslk(share->servers_mutex());

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
			share->servers()[srv->addr()] = *it;
		}
	}
	share->new_servers().clear();

	sslk.unlock();
	nslk.unlock();

	net->scope_proto_network().sync_hash_space_partner(hslk);
	//net->scope_proto_network().sync_hash_space_servers();
	//push_hash_space_clients();
}

void proto_replace::detach_fault_servers(REQUIRE_HSLK)
{
	ClockTime ct = net->clock_incr_clocktime();

	share->whs().remove_fault_servers(ct);

	net->scope_proto_network().sync_hash_space_partner(hslk);
	//net->scope_proto_network().sync_hash_space_servers();
	//net->scope_proto_network().push_hash_space_clients();
}


proto_replace::progress::progress() :
	m_clocktime(0) { }

proto_replace::progress::~progress() { }

inline ClockTime proto_replace::progress::clocktime() const
{
	return m_clocktime;
}

inline void proto_replace::progress::reset(ClockTime replace_time, const nodes_t& nodes)
{
	m_target_nodes = m_remainder = nodes;
	m_clocktime = replace_time;
}

bool proto_replace::progress::pop(ClockTime replace_time, const rpc::address& node)
{
	if(m_clocktime != replace_time) { return false; }
	if(m_remainder.empty()) { return false; }

	nodes_t::iterator erase_from =
		std::remove(m_remainder.begin(), m_remainder.end(), node);
	m_remainder.erase(erase_from, m_remainder.end());

	return m_remainder.empty();
}

proto_replace::progress::nodes_t proto_replace::progress::invalidate()
{
	m_clocktime = ClockTime(0);
	m_remainder.clear();

	nodes_t tmp;
	m_target_nodes.swap(tmp);
	return tmp;
}


void proto_replace::start_replace(REQUIRE_HSLK)
{
	LOG_INFO("start replace copy");
	pthread_scoped_lock relk(m_replace_mutex);

	shared_zone life(new msgpack::zone());

	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(share->whs());
	ClockTime replace_time(share->whs().clocktime());

	server::proto_replace::ReplaceCopyStart_1 param(*seed, net->clock_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(proto_replace, ReplaceCopyStart_1) );

	progress::nodes_t target_nodes;

	pthread_scoped_lock sslk(share->servers_mutex());
	EACH_ACTIVE_SERVERS_BEGIN(n)
		n->call(param, life, callback, 10);
		target_nodes.push_back(n->addr());
	EACH_ACTIVE_SERVERS_END
	sslk.unlock();

	LOG_INFO("active node: ",target_nodes.size());
	m_copying.reset(replace_time, target_nodes);
	m_deleting.invalidate();
	relk.unlock();

	// push hashspace to the clients
	try {
		net->scope_proto_network().push_hash_space_clients(hslk);
	} catch (std::runtime_error& e) {
		LOG_ERROR("HashSpacePush failed: ",e.what());
	} catch (...) {
		LOG_ERROR("HashSpacePush failed: unknown error");
	}
}

RPC_REPLY_IMPL(proto_replace, ReplaceCopyStart_1, from, res, err, life)
{
	// FIXME
}


RPC_IMPL(proto_replace, ReplaceElection_1, req, z, response)
{
	LOG_DEBUG("ReplaceElection");

	if(req.node()->addr() != share->partner()) {
		throw std::runtime_error("unknown partner node");
	}

	net->clock_update(req.param().clock);

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



RPC_IMPL(proto_replace, ReplaceCopyEnd_1, req, z, response)
{
	pthread_scoped_lock relk(m_replace_mutex);

	net->clock_update(req.param().clock);

	ClockTime replace_time(req.param().replace_time);
	if(m_copying.pop(replace_time, req.node()->addr())) {
		finish_replace_copy(relk);
	}

	relk.unlock();
	response.result(true);
}


RPC_IMPL(proto_replace, ReplaceDeleteEnd_1, req, z, response)
{
	pthread_scoped_lock relk(m_replace_mutex);

	net->clock_update(req.param().clock);

	ClockTime replace_time(req.param().replace_time);
	if(m_deleting.pop(replace_time, req.node()->addr())) {
		finish_replace(relk);
	}

	relk.unlock();
	response.result(true);
}


void proto_replace::finish_replace_copy(REQUIRE_RELK)
{
	ClockTime replace_time = m_copying.clocktime();
	LOG_INFO("start replace delete time(",replace_time.get(),")");

	progress::nodes_t target_nodes = m_copying.invalidate();

	shared_zone life(new msgpack::zone());
	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(share->whs());
	// FIXME server::proto_replace::ReplaceDeleteStart_1 has HashSpace::Seed:
	//       not so good efficiency
	server::proto_replace::ReplaceDeleteStart_1 param(*seed, net->clock_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(proto_replace, ReplaceDeleteStart_1) );

	pthread_scoped_lock sslk(share->servers_mutex());
	for(progress::nodes_t::iterator it(target_nodes.begin()),
			it_end(target_nodes.end()); it != it_end; ++it) {
		net->get_node(*it)->call(param, life, callback, 10);
	}

	m_deleting.reset(replace_time, target_nodes);

	pthread_scoped_lock hslk(share->hs_mutex());
	share->rhs() = share->whs();
	net->scope_proto_network().push_hash_space_clients(hslk);
	hslk.unlock();
}

RPC_REPLY_IMPL(proto_replace, ReplaceDeleteStart_1, from, res, err, life)
{
	// FIXME
}


inline void proto_replace::finish_replace(REQUIRE_RELK)
{
	LOG_INFO("replace finished time(",m_deleting.clocktime().get(),")");
	m_deleting.invalidate();
}


}  // namespace manager
}  // namespace kumo

