#include "logic/mgr_impl.h"
#include <algorithm>

namespace kumo {


void Manager::add_server(const address& addr, shared_node& s)
{
	LOG_INFO("server connected ",s->addr());

	//if(!m_whs.server_is_fault(addr)) {
		m_newcomer_servers.push_back( weak_node(s) );
	//}

	if(m_cfg_auto_replace) {
		// delayed replace
		delayed_replace_election();
	}
}

void Manager::remove_server(const address& addr)
{
	LOG_INFO("server lost ",addr);

	ClockTime ct = m_clock.now_incr();
	bool wfault = m_whs.fault_server(ct, addr);
	bool rfault = m_rhs.fault_server(ct, addr);
	if(wfault || rfault) {
		if(!m_cfg_auto_replace) {
			sync_hash_space_partner();
			sync_hash_space_servers();
			push_hash_space_clients();
		}
	}

	m_servers.erase(addr);

	for(newcomer_servers_t::iterator it(m_newcomer_servers.begin());
			it != m_newcomer_servers.end(); ) {
		shared_node n(it->lock());
		if(!n || n->addr() == addr) {
			it = m_newcomer_servers.erase(it);
		} else {
			++it;
		}
	}

	if(m_cfg_auto_replace) {
		// delayed replace
		delayed_replace_election();
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
	attach_new_servers();
	detach_fault_servers();
	if(m_partner.connectable() && m_partner < addr()) {
		LOG_INFO("replace delegate to ",m_partner);
		try {
			// delegate replace
			shared_zone life(new mp::zone());
			// FIXME protocol::type::HashSpace has HashSpace::Seed:
			//       not so good efficiency
			HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(m_whs);
			protocol::type::ReplaceElection arg(*seed, m_clock.get_incr());
			get_node(m_partner)->call(
					protocol::ReplaceElection, arg, life,
					BIND_RESPONSE(ResReplaceElection), 10);
		} catch (...) {
			LOG_ERROR("replace delegate procedure failed");
			start_replace();
		}
	} else {
		LOG_INFO("replace self elected");
		start_replace();
	}
}

RPC_REPLY(ResReplaceElection, from, res, err, life)
{
	if(!err.is_nil() || res.is_nil()) {
		LOG_INFO("replace delegate failed, elected");
		start_replace();
	} else {
		// do nothing
	}
}



void Manager::attach_new_servers()
{
	// update hash space
	ClockTime ct = m_clock.now_incr();
	LOG_INFO("update hash space at time(",ct.get(),")");
	for(newcomer_servers_t::iterator it(m_newcomer_servers.begin()), it_end(m_newcomer_servers.end());
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
	m_newcomer_servers.clear();
	sync_hash_space_partner();
	//sync_hash_space_servers();
	//push_hash_space_clients();
}

void Manager::detach_fault_servers()
{
	ClockTime ct = m_clock.now_incr();
	m_whs.remove_fault_servers(ct);
	sync_hash_space_partner();
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

inline bool Manager::ReplaceContext::empty() const
{
	return m_num == 0;
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


void Manager::start_replace()
{
	LOG_INFO("start replace copy");

	shared_zone life(new mp::zone());
	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(m_whs);
	// FIXME protocol::type::ReplaceCopyStart has HashSpace::Seed:
	//       not so good efficiency
	protocol::type::ReplaceCopyStart arg(*seed, m_clock.get_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(ResReplaceCopyStart) );

	unsigned int num_active = 0;
	EACH_ACTIVE_SERVERS_BEGIN(n)
		n->call(protocol::ReplaceCopyStart, arg, life, callback, 10);
		++num_active;
	EACH_ACTIVE_SERVERS_END

	m_copying.reset(m_whs.clocktime(), num_active);
	m_deleting.reset(0, 0);

	// push hashspace to the clients
	try {
		push_hash_space_clients();
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
			attach_new_servers();
			detach_fault_servers();
			start_replace();
			response.result(true);
		}
	}
}
RPC_CATCH(ReplaceElection, response)



CLUSTER_FUNC(ReplaceCopyEnd, from, response, life, param)
try {
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
	// FIXME
	ClockTime clocktime = m_copying.clocktime();
	LOG_INFO("start replace delete time(",clocktime.get(),")");
	m_copying.reset(0, 0);

	shared_zone life(new mp::zone());
	HashSpace::Seed* seed = life->allocate<HashSpace::Seed>(m_whs);
	// FIXME protocol::type::ReplaceDeleteStart has HashSpace::Seed:
	//       not so good efficiency
	protocol::type::ReplaceDeleteStart arg(*seed, m_clock.get_incr());

	using namespace mp::placeholders;
	rpc::callback_t callback( BIND_RESPONSE(ResReplaceDeleteStart) );

	unsigned int num_active = 0;
	EACH_ACTIVE_SERVERS_BEGIN(node)
		node->call(protocol::ReplaceDeleteStart, arg, life, callback, 10);
		++num_active;
	EACH_ACTIVE_SERVERS_END

	m_deleting.reset(clocktime, num_active);
}

RPC_REPLY(ResReplaceDeleteStart, from, res, err, life)
{
	// FIXME
}


inline void Manager::finish_replace()
{
	// FIXME
	LOG_INFO("replace finished time(",m_deleting.clocktime().get(),")");
	m_deleting.reset(0, 0);
	m_rhs = m_whs;
}


}  // namespace kumo

