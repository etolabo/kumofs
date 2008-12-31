#include "logic/mgr_impl.h"
#include <algorithm>

namespace kumo {


void Manager::add_server(const address& addr, shared_node& s)
{
	LOG_INFO("server connected ",s->addr());

	//if(!m_whs.server_is_fault(addr)) {
	pthread_scoped_lock nslk(m_new_servers_mutex);
	m_new_servers.push_back( weak_node(s) );
	nslk.unlock();

	if(m_cfg_auto_replace) {
		// delayed replace
		delayed_replace_election();
	}
}

void Manager::remove_server(const address& addr)
{
	LOG_INFO("server lost ",addr);

	ClockTime ct = m_clock.now_incr();

	pthread_scoped_lock hslk(m_hs_mutex);
	pthread_scoped_lock slk(m_servers_mutex);
	pthread_scoped_lock nslk(m_new_servers_mutex);

	bool wfault = m_whs.fault_server(ct, addr);
	bool rfault = m_rhs.fault_server(ct, addr);

	if((wfault || rfault) && !m_cfg_auto_replace) {
		sync_hash_space_partner(hslk);
		sync_hash_space_servers(hslk);
		push_hash_space_clients(hslk);
	}
	hslk.unlock();

	m_servers.erase(addr);
	slk.unlock();

	for(new_servers_t::iterator it(m_new_servers.begin());
			it != m_new_servers.end(); ) {
		shared_node n(it->lock());
		if(!n || n->addr() == addr) {
			it = m_new_servers.erase(it);
		} else {
			++it;
		}
	}
	nslk.unlock();

	if(m_cfg_auto_replace) {
		// delayed replace
		delayed_replace_election();
	} else {
		pthread_scoped_lock relk(m_replace_mutex);
		m_copying.invalidate();  // prevent replace delete
	}
}


void Manager::delayed_replace_election()
{
	m_delayed_replace_clock = m_cfg_replace_delay_clocks;
	LOG_INFO("set delayed replace clock(",m_delayed_replace_clock,")");
	if(m_delayed_replace_clock == 0) {
		m_delayed_replace_clock = 1;
	}
}


void Manager::replace_election()
{
	// XXX
	// election: smaller address has priority
	pthread_scoped_lock hslk(m_hs_mutex);
	attach_new_servers(hslk);
	detach_fault_servers(hslk);

	if(m_partner.connectable() && m_partner < addr()) {
		LOG_INFO("replace delegate to ",m_partner);
	
		// delegate replace
		shared_zone life(new msgpack::zone());

		HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(m_whs);
		hslk.unlock();

		protocol::type::ReplaceElection arg(*seed, m_clock.get_incr());
		get_node(m_partner)->call(  // FIXME exception
				protocol::ReplaceElection, arg, life,
				BIND_RESPONSE(ResReplaceElection), 10);
	} else {
		LOG_INFO("replace self elected");
		start_replace(hslk);
	}
}

RPC_REPLY(ResReplaceElection, from, res, err, life)
{
	if(!err.is_nil() || res.is_nil()) {
		LOG_INFO("replace delegate failed, elected");
		pthread_scoped_lock hslk(m_hs_mutex);
		start_replace(hslk);
	} else {
		// do nothing
	}
}



void Manager::attach_new_servers(const pthread_scoped_lock& hslk)
{
	// update hash space
	ClockTime ct = m_clock.now_incr();
	LOG_INFO("update hash space at time(",ct.get(),")");

	pthread_scoped_lock nslk(m_new_servers_mutex);
	pthread_scoped_lock slk(m_servers_mutex);

	for(new_servers_t::iterator it(m_new_servers.begin()), it_end(m_new_servers.end());
			it != it_end; ++it) {
		shared_node srv(it->lock());
		if(srv) {
			if(m_whs.server_is_include(srv->addr())) {
				LOG_INFO("recover server: ",srv->addr());
				m_whs.recover_server(ct, srv->addr());
			} else {
				LOG_INFO("new server: ",srv->addr());
				m_whs.add_server(ct, srv->addr());
			}
			m_servers[srv->addr()] = *it;
		}
	}
	m_new_servers.clear();

	slk.unlock();
	nslk.unlock();

	sync_hash_space_partner(hslk);
	//sync_hash_space_servers();
	//push_hash_space_clients();
}

void Manager::detach_fault_servers(const pthread_scoped_lock& hslk)
{
	ClockTime ct = m_clock.now_incr();

	m_whs.remove_fault_servers(ct);

	sync_hash_space_partner(hslk);
	//sync_hash_space_servers();
	//push_hash_space_clients();
}


Manager::ReplaceContext::ReplaceContext() :
	m_num(0), m_clocktime(0) {}

Manager::ReplaceContext::~ReplaceContext() {}

inline ClockTime Manager::ReplaceContext::clocktime() const { return m_clocktime; }

inline void Manager::ReplaceContext::reset(ClockTime ct, unsigned int num)
{
	m_num = num;
	m_clocktime = ct;
}

bool Manager::ReplaceContext::pop(ClockTime ct)
{
	if(m_clocktime != ct) { return false; }
	if(m_num == 1) {
		m_num = 0;
		return true;
	}
	--m_num;
	return false;
}

void Manager::ReplaceContext::invalidate()
{
	m_clocktime = ClockTime(0);
	m_num = 0;
}


void Manager::start_replace(const pthread_scoped_lock& hslk)
{
	LOG_INFO("start replace copy");
	pthread_scoped_lock relk(m_replace_mutex);

	shared_zone life(new msgpack::zone());

	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(m_whs);
	ClockTime ct(m_whs.clocktime());

	protocol::type::ReplaceCopyStart arg(*seed, m_clock.get_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(ResReplaceCopyStart) );

	unsigned int num_active = 0;

	pthread_scoped_lock slk(m_servers_mutex);
	EACH_ACTIVE_SERVERS_BEGIN(n)
		n->call(protocol::ReplaceCopyStart, arg, life, callback, 10);
		++num_active;
	EACH_ACTIVE_SERVERS_END
	slk.unlock();

	m_copying.reset(ct, num_active);
	m_deleting.reset(0, 0);

	// push hashspace to the clients
	try {
		push_hash_space_clients(hslk);
	} catch (std::runtime_error& e) {
		LOG_ERROR("HashSpacePush failed: ",e.what());
	} catch (...) {
		LOG_ERROR("HashSpacePush failed: unknown error");
	}
}

RPC_REPLY(ResReplaceCopyStart, from, res, err, life)
{
	// FIXME
}


CLUSTER_FUNC(ReplaceElection, from, response, life, param)
try {
	LOG_DEBUG("ReplaceElection");

	if(from->addr() != m_partner) {
		throw std::runtime_error("unknown partner node");
	}

	m_clock.update(param.clock());

	pthread_scoped_lock hslk(m_hs_mutex);
	ClockTime ct(m_whs.clocktime());

	if(param.hsseed().empty() ||
			ClockTime(param.hsseed().clocktime()) < m_whs.clocktime()) {
		LOG_DEBUG("obsolete hashspace");
		response.result(true);
		return;
	}

	if(m_whs.clocktime() < param.hsseed().clocktime()) {
		LOG_INFO("double replace guard ",m_partner);

	} else {
		// election: smaller address has priority
		if(m_partner < addr()) {
			LOG_INFO("replace re-delegate to ",m_partner);
			response.null();
		} else {
			LOG_INFO("replace delegated from ",m_partner);
			attach_new_servers(hslk);
			detach_fault_servers(hslk);
			start_replace(hslk);
			hslk.unlock();
			response.result(true);
		}
	}
}
RPC_CATCH(ReplaceElection, response)



CLUSTER_FUNC(ReplaceCopyEnd, from, response, life, param)
try {
	pthread_scoped_lock relk(m_replace_mutex);

	m_clock.update(param.clock());

	ClockTime ct(param.clocktime());
	if(m_copying.pop(ct)) {
		finish_replace_copy();
	}

	response.result(true);
}
RPC_CATCH(ReplaceCopyEnd, response)


CLUSTER_FUNC(ReplaceDeleteEnd, from, response, life, param)
try {
	pthread_scoped_lock relk(m_replace_mutex);

	m_clock.update(param.clock());

	ClockTime ct(param.clocktime());
	if(m_deleting.pop(ct)) {
		finish_replace();
	}

	response.result(true);
}
RPC_CATCH(ReplaceDeleteEnd, response)


void Manager::finish_replace_copy()
{
	// called from ReplaceDeleteEnd

	// FIXME
	ClockTime clocktime = m_copying.clocktime();
	LOG_INFO("start replace delete time(",clocktime.get(),")");
	m_copying.reset(0, 0);

	shared_zone life(new msgpack::zone());
	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(m_whs);
	// FIXME protocol::type::ReplaceDeleteStart has HashSpace::Seed:
	//       not so good efficiency
	protocol::type::ReplaceDeleteStart arg(*seed, m_clock.get_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(ResReplaceDeleteStart) );

	unsigned int num_active = 0;

	pthread_scoped_lock slk(m_servers_mutex);
	EACH_ACTIVE_SERVERS_BEGIN(node)
		node->call(protocol::ReplaceDeleteStart, arg, life, callback, 10);
		++num_active;
	EACH_ACTIVE_SERVERS_END
	slk.unlock();

	m_deleting.reset(clocktime, num_active);

	pthread_scoped_lock hslk(m_hs_mutex);
	m_rhs = m_whs;
	hslk.unlock();
}

RPC_REPLY(ResReplaceDeleteStart, from, res, err, life)
{
	// FIXME
}


inline void Manager::finish_replace()
{
	// called from ReplaceDeleteEnd

	// FIXME
	LOG_INFO("replace finished time(",m_deleting.clocktime().get(),")");
	m_deleting.reset(0, 0);
}


}  // namespace kumo

