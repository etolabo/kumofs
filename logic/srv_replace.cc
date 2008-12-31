#include "logic/srv_impl.h"
#include <algorithm>

namespace kumo {


bool Server::test_replicator_assign(HashSpace& hs, uint64_t h, const address& target)
{
	EACH_ASSIGN(hs, h, r,
			if(r.is_active()) {  // don't write to fault node
				if(r.addr() == target) return true;
			})
	return false;
}


Server::ReplaceContext::ReplaceContext() :
	m_propose_waiting(0),
	m_push_waiting(0),
	m_clocktime(0) {}

Server::ReplaceContext::~ReplaceContext() {}

inline void Server::ReplaceContext::reset(const address& mgr, ClockTime ct)
{
	m_propose_waiting = 0;
	m_push_waiting = 0;
	m_clocktime = ct;
	m_mgr = mgr;
}

inline void Server::ReplaceContext::proposed(ClockTime ct)
	{ if(ct == m_clocktime) { ++m_propose_waiting; } }
inline void Server::ReplaceContext::propose_returned(ClockTime ct)
	{ if(ct == m_clocktime) { --m_propose_waiting; } }
inline void Server::ReplaceContext::pushed(ClockTime ct)
	{ if(ct == m_clocktime) { ++m_push_waiting; } }
inline void Server::ReplaceContext::push_returned(ClockTime ct)
	{ if(ct == m_clocktime) { --m_push_waiting; } }

inline bool Server::ReplaceContext::is_finished(ClockTime ct) const
	{ return m_clocktime == ct && m_propose_waiting == 0 && m_push_waiting == 0; }

inline void Server::ReplaceContext::invalidate()
	{ m_propose_waiting = -1; m_push_waiting = -1; }

inline const address& Server::ReplaceContext::mgr_addr() const
	{ return m_mgr; }



CLUSTER_FUNC(ReplaceCopyStart, from, response, z, param)
try {
	m_clock.update(param.clock());

	HashSpace hs(param.hsseed());

	response.result(true);

	try {
		replace_copy(from->addr(), hs);
	} catch (std::runtime_error& e) {
		LOG_ERROR("replace copy failed: ",e.what());
	} catch (...) {
		LOG_ERROR("replace copy failed: unknown error");
	}
}
RPC_CATCH(ReplaceCopyStart, response)


CLUSTER_FUNC(ReplaceDeleteStart, from, response, z, param)
try {
	m_clock.update(param.clock());

	HashSpace hs(param.hsseed());

	response.result(true);

	try {
		replace_delete(from, hs);
	} catch (std::runtime_error& e) {
		LOG_ERROR("replace delete failed: ",e.what());
	} catch (...) {
		LOG_ERROR("replace delete failed: unknown error");
	}
}
RPC_CATCH(ReplaceDeleteStart, response)



void Server::replace_copy(const address& manager_addr, HashSpace& hs)
{
	ClockTime replace_time = hs.clocktime();

	{
		pthread_scoped_lock relk(m_replacing_mutex);
		m_replacing.reset(manager_addr, replace_time);
	}

	LOG_INFO("start replace copy for time(",replace_time.get(),")");

	pthread_scoped_wrlock whlock(m_whs_mutex);
	HashSpace srchs(m_whs);
	m_whs = hs;
	whlock.unlock();

	HashSpace& dsths(hs);

	typedef std::vector<address> addrs_t;
	typedef addrs_t::iterator addrs_it;

	addrs_t fault_nodes;
{
	addrs_t src_nodes;
	addrs_t dst_nodes;

	srchs.get_active_nodes(src_nodes);
	dsths.get_active_nodes(dst_nodes);

	for(addrs_it it(src_nodes.begin()); it != src_nodes.end(); ++it) {
		LOG_INFO("src active node: ",*it);
	}
	for(addrs_it it(dst_nodes.begin()); it != dst_nodes.end(); ++it) {
		LOG_INFO("dst active node: ",*it);
	}

	if(src_nodes.empty() || dst_nodes.empty()) {
		LOG_INFO("empty hash space. skip replacing.");
		goto skip_replace;
	}

	std::sort(src_nodes.begin(), src_nodes.end());
	std::sort(dst_nodes.begin(), dst_nodes.end());

	for(addrs_it it(src_nodes.begin()); it != src_nodes.end(); ++it) {
		if(!std::binary_search(dst_nodes.begin(), dst_nodes.end(), *it)) {
			fault_nodes.push_back(*it);
		}
	}

	for(addrs_it it(fault_nodes.begin()); it != fault_nodes.end(); ++it) {
		LOG_INFO("fault node: ",*it);
	}

	if(std::binary_search(fault_nodes.begin(), fault_nodes.end(), addr())) {
		LOG_WARN("I'm marked as fault. skip replacing.");
		goto skip_replace;
	}
}


{
	addrs_t Sa;
	addrs_t Da;
	addrs_t current_owners;
	addrs_t newbies;
	Sa.reserve(NUM_REPLICATION+1);
	Da.reserve(NUM_REPLICATION+1);
	current_owners.reserve(NUM_REPLICATION+1);
	newbies.reserve(NUM_REPLICATION+1);

	// only one thread can use iterator
	pthread_scoped_wrlock dblk(m_db.mutex());

	Storage::iterator kv;
	m_db.iterator_init(kv);
	while(m_db.iterator_next(kv)) {
		const char* raw_key = kv.key();
		size_t raw_keylen = kv.keylen();
		const char* raw_val = kv.val();
		size_t raw_vallen = kv.vallen();

		if(raw_vallen < DBFormat::VALUE_META_SIZE) { continue; }
		if(raw_keylen < DBFormat::KEY_META_SIZE) { continue; }

		uint64_t h = DBFormat::hash(kv.key());

		Sa.clear();
		EACH_ASSIGN(srchs, h, r, {
			if(r.is_active()) Sa.push_back(r.addr()); });

		Da.clear();
		EACH_ASSIGN(dsths, h, r, {
			if(r.is_active()) Da.push_back(r.addr()); });

		current_owners.clear();
		for(addrs_it it(Sa.begin()); it != Sa.end(); ++it) {
			if(!std::binary_search(fault_nodes.begin(), fault_nodes.end(), *it)) {
				current_owners.push_back(*it);
			}
		}

		if(current_owners.empty() || current_owners.front() != addr()) { continue; }

		newbies.clear();
		for(addrs_it it(Da.begin()); it != Da.end(); ++it) {
			if(std::find(Sa.begin(), Sa.end(), *it) == Sa.end()) {
				newbies.push_back(*it);
			}
		}

		shared_zone life(new msgpack::zone());
		kv.release_key(*life);
		if(raw_vallen > 512) {   // FIXME
			// propose -> push
			uint64_t clocktime = DBFormat::clocktime(raw_val);
			for(addrs_it it(newbies.begin()); it != newbies.end(); ++it) {
				propose_replace_push(*it, raw_key, raw_keylen, clocktime, life, replace_time);
			}

		} else {
			// push directly
			kv.release_val(*life);
			for(addrs_it it(newbies.begin()); it != newbies.end(); ++it) {
				replace_push(*it, raw_key, raw_keylen, raw_val, raw_vallen, life, replace_time);
			}
		}
	}
}

skip_replace:
	pthread_scoped_lock relk(m_replacing_mutex);
	if(m_replacing.is_finished(replace_time)) {
		finish_replace_copy(replace_time, relk);
	}
}


inline void Server::propose_replace_push(const address& node,
		const char* raw_key, uint32_t raw_keylen,
		uint64_t metaval_clocktime, shared_zone& life,
		ClockTime replace_time)
{
	RetryReplacePropose* retry = life->allocate<RetryReplacePropose>(
			protocol::type::ReplacePropose(raw_key, raw_keylen, metaval_clocktime)
			);

	retry->set_callback( BIND_RESPONSE(ResReplacePropose, retry, replace_time) );

	retry->call(get_node(node), life, 10);

	pthread_scoped_lock relk(m_replacing_mutex);
	m_replacing.proposed(replace_time);
}

inline void Server::replace_push(const address& node,
		const char* raw_key, uint32_t raw_keylen,
		const char* raw_val, size_t raw_vallen,
		shared_zone& life, ClockTime replace_time)
{
	if(!raw_val || raw_vallen < DBFormat::VALUE_META_SIZE) { return; }

	RetryReplacePush* retry = life->allocate<RetryReplacePush>(
			protocol::type::ReplacePush(raw_key, raw_keylen, raw_val, raw_vallen)
			);

	retry->set_callback( BIND_RESPONSE(ResReplacePush, retry, replace_time) );

	retry->call(get_node(node), life, 10);

	pthread_scoped_lock relk(m_replacing_mutex);
	m_replacing.pushed(replace_time);
}



CLUSTER_FUNC(ReplacePropose, from, response, z, param)
try {
	pthread_scoped_rdlock whlk(m_whs_mutex);

	if(m_whs.empty()) {
		//throw std::runtime_error("server not ready");
		// don't send response if server not ready.
		// this makes sender be timeout and it will retry
		// after several seconds.
		return;
	}

	protocol::type::DBKey key(param.dbkey());

// FIXME ReplacePropose may go ahead of ReplaceCopyStart
//       This node may have old hash space while proposer have new one
//	if(!test_replicator_assign(m_whs, key.hash(), addr())) {
//		// ignore obsolete hash space error
//		response.null();
//		return;
//	}

	whlk.unlock();

	pthread_scoped_rdlock dblk(m_db.mutex());
	uint64_t clocktime;
	bool stored = DBFormat::get_clocktime(m_db,
			key.raw_data(), key.raw_size(), &clocktime);
	dblk.unlock();

	if(!stored || ClockTime(clocktime) < ClockTime(param.clocktime())) {
		// key is not stored OR stored key is old
		// require replication
		response.result(true);

	} else {
		// key is already deleted while replacing
		response.null();
	}
}
RPC_CATCH(ReplacePropose, response)


CLUSTER_FUNC(ReplacePush, from, response, z, param)
try {
	pthread_scoped_rdlock whlk(m_whs_mutex);

	if(m_whs.empty()) {
		//throw std::runtime_error("server not ready");
		// don't send response if server not ready.
		// this makes sender timeout and it will retry
		// after several seconds.
		return;
	}

	protocol::type::DBKey key(param.dbkey());
	protocol::type::DBValue val(param.dbval());

// FIXME ReplacePush may go ahead of ReplaceCopyStart
//       This node may have old hash space while proposer have new one
//	if(!test_replicator_assign(m_whs, key.hash(), addr())) {
//		// ignore obsolete hash space error
//		response.null();
//		return;
//	}

	whlk.unlock();

	pthread_scoped_wrlock dblk(m_db.mutex());
	uint64_t clocktime;
	bool stored = DBFormat::get_clocktime(m_db,
			key.raw_data(), key.raw_size(), &clocktime);

	if(!stored || ClockTime(clocktime) < ClockTime(val.clocktime())) {
		// key is not stored OR stored key is old
		m_db.set(key.raw_data(), key.raw_size(),
				 val.raw_data(), val.raw_size());
		dblk.unlock();
		response.result(true);

	} else {
		// key is already deleted while replacing
		// do nothing
		LOG_TRACE("obsolete or same replace push");
		response.null();
	}
}
RPC_CATCH(ReplacePush, response)


RPC_REPLY(ResReplacePropose, from, res, err, life,
		RetryReplacePropose* retry, ClockTime replace_time)
{
	// retry if failed
	if(!err.is_nil()) {
		if(SESSION_IS_ACTIVE(from)) {
			// FIXME delayed retry
			if(retry->retry_incr(m_cfg_replace_propose_retry_num)) {
				retry->call(from, life);
				LOG_DEBUG("ReplacePropose failed: ",err,", retry ",retry->num_retried());
				return;
			}
		}
		LOG_ERROR("ReplacePropose failed: ",err);
	}

	if(!res.is_nil() && SESSION_IS_ACTIVE(from)) {
		protocol::type::DBKey key(retry->param().dbkey());
		uint32_t raw_vallen;

		pthread_scoped_rdlock dblk(m_db.mutex());
		const char* raw_val = m_db.get(key.raw_data(), key.raw_size(),
				&raw_vallen, *life);
		dblk.unlock();

		replace_push(
				mp::static_pointer_cast<rpc::node>(from)->addr(),  // FIXME
				key.raw_data(), key.raw_size(), raw_val, raw_vallen,
				life, replace_time);
	}

	pthread_scoped_lock relk(m_replacing_mutex);

	m_replacing.propose_returned(replace_time);

	if(m_replacing.is_finished(replace_time)) {
		finish_replace_copy(replace_time, relk);
	}
}

RPC_REPLY(ResReplacePush, from, res, err, life,
		RetryReplacePush* retry, ClockTime replace_time)
{
	// retry if failed
	if(!err.is_nil()) {
		if(SESSION_IS_ACTIVE(from)) {
			// FIXME delayed retry
			if(retry->retry_incr(m_cfg_replace_push_retry_num)) {
				retry->call(from, life);
				LOG_DEBUG("ReplacePush failed: ",err,", retry ",retry->num_retried());
				return;
			}
		}
		LOG_ERROR("ReplacePush failed: ",err);
	}

	pthread_scoped_lock relk(m_replacing_mutex);

	m_replacing.push_returned(replace_time);

	if(m_replacing.is_finished(replace_time)) {
		finish_replace_copy(replace_time, relk);
	}
}




void Server::finish_replace_copy(ClockTime replace_time, REQUIRE_RELK)
{
	shared_zone nullz;
	protocol::type::ReplaceCopyEnd arg(replace_time.get(), m_clock.get_incr());

	address addr;
	//{
	//	pthread_scoped_lock relk(m_replacing_mutex);
		addr = m_replacing.mgr_addr();
	//	m_replacing.invalidate();
	//}

	using namespace mp::placeholders;
	get_node(addr)->call(
			protocol::ReplaceCopyEnd, arg, nullz,
			BIND_RESPONSE(ResReplaceCopyEnd), 10);
}

RPC_REPLY(ResReplaceCopyEnd, from, res, err, life)
{
	if(!err.is_nil()) { LOG_ERROR("ReplaceCopyEnd failed: ",err); }
	// FIXME
}


void Server::replace_delete(shared_node& manager, HashSpace& hs)
{
	pthread_scoped_rdlock whlk(m_whs_mutex);

	pthread_scoped_wrlock rhlk(m_rhs_mutex);
	m_rhs = m_whs;
	rhlk.unlock();

	LOG_INFO("start replace delete for time(",m_whs.clocktime().get(),")");

	if(!m_whs.empty()) {
		Storage::iterator kv;
		pthread_scoped_wrlock dblk(m_db.mutex());
		m_db.iterator_init(kv);
		while(m_db.iterator_next(kv)) {
			if(kv.keylen() < DBFormat::KEY_META_SIZE ||
					kv.vallen() < DBFormat::VALUE_META_SIZE) {
				LOG_TRACE("delete invalid key: ",kv.key());
				m_db.del(kv.key(), kv.keylen());
			}
			uint64_t h = DBFormat::hash(kv.key());
			if(!test_replicator_assign(m_whs, h, addr())) {
				LOG_TRACE("replace delete key: ",kv.key());
				m_db.del(kv.key(), kv.keylen());
			}
		}
	}

	shared_zone nullz;
	protocol::type::ReplaceDeleteEnd arg(m_whs.clocktime().get(), m_clock.get_incr());
	using namespace mp::placeholders;
	manager->call(protocol::ReplaceDeleteEnd, arg, nullz,
			BIND_RESPONSE(ResReplaceDeleteEnd), 10);

	LOG_INFO("finish replace for time(",m_whs.clocktime().get(),")");
}

RPC_REPLY(ResReplaceDeleteEnd, from, res, err, life)
{
	if(!err.is_nil()) { LOG_ERROR("ReplaceDeleteEnd failed: ",err); }
	// FIXME
}


}  // namespace kumo

