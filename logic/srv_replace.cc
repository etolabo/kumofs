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


namespace {
	struct ReplaceProposePool {
		typedef protocol::type::ReplacePropose type;
		ReplaceProposePool(address a) : addr(a) { }
		address addr;
		type request;
	};

	struct ReplacePushPool {
		typedef protocol::type::ReplacePush type;
		ReplacePushPool(address a) : addr(a) { }
		address addr;
		type request;
	};
}  // noname namespace

void Server::replace_copy(const address& manager_addr, HashSpace& hs)
{
	ClockTime replace_time = hs.clocktime();

	{
		pthread_scoped_lock relk(m_replacing_mutex);
		m_replacing.reset(manager_addr, replace_time);
		m_replacing.pushed(replace_time);  // replace_copy
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

	typedef std::vector<ReplaceProposePool> propose_pool_t;
	typedef std::vector<ReplacePushPool> push_pool_t;
	propose_pool_t propose_pool;
	push_pool_t push_pool;

	shared_zone propose_life(new msgpack::zone());
	shared_zone push_life(new msgpack::zone());
	size_t pool_size = 0;
	size_t pool_num  = 0;

	// FIXME
	//// only one thread can use iterator
	//pthread_scoped_lock itlk(m_db.iter_mutex());
	//// read-lock database
	//pthread_scoped_rdlock dblk(m_db.mutex());

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

		if(newbies.empty()) { continue; }

		#define POOL_REQUEST(TYPE, POOL, ELEMENT) \
			for(addrs_t::iterator it(newbies.begin());  it != newbies.end(); ++it) { \
				for(TYPE::iterator p(POOL.begin()); p != POOL.end(); ++p) { \
					if(p->addr == *it) { \
						p->request.push_back(ELEMENT); \
						goto pool_next_ ## TYPE; \
					} \
				} \
				POOL.push_back( TYPE::value_type(*it) ); \
				POOL.back().request.push_back(ELEMENT); \
				pool_next_ ## TYPE: { } \
			}

		if(raw_vallen > 512) {   // FIXME
			// propose -> push
			uint64_t clocktime = DBFormat::clocktime(raw_val);
			kv.release_key(*propose_life);

			protocol::type::ReplaceProposeElement e(
					raw_key, raw_keylen, clocktime);
			POOL_REQUEST(propose_pool_t, propose_pool, e);
			//pool_size += (raw_keylen + 64) * newbies.size();  // FIXME 64
			pool_size += (raw_keylen + raw_vallen + 64) * newbies.size();  // FIXME 64
			pool_num  += newbies.size();

		} else {
			// push directly
#if 0
			char* nbuf = (char*)push_life->malloc(raw_keylen + raw_vallen);
			memcpy(nbuf, raw_key, raw_keylen);
			memcpy(nbuf+raw_keylen, raw_val, raw_vallen);

			protocol::type::ReplacePushElement e(
					nbuf, raw_keylen,
					nbuf+raw_keylen, raw_vallen);
#else
			kv.release_key(*push_life);
			kv.release_val(*push_life);

			protocol::type::ReplacePushElement e(
					raw_key, raw_keylen,
					raw_val, raw_vallen);
#endif
			POOL_REQUEST(push_pool_t, push_pool, e);
			pool_size += (raw_keylen + raw_vallen + 64) * newbies.size();  // FIXME 64
			pool_num  += newbies.size() * 2;
		}

		#define FLUSH_POOL(FUNC, TYPE, POOL, LIFE) \
			for(TYPE::iterator it(POOL.begin()); \
					it != POOL.end(); ++it) { \
				FUNC(it->addr, it->request, LIFE, replace_time); \
			} \
			POOL.clear();

		// FIXME pool_num limit
		if(pool_num > 10240 || pool_size/1024/1024 > m_cfg_replace_pool_size) {
			FLUSH_POOL(send_replace_propose, propose_pool_t, propose_pool, propose_life);
			FLUSH_POOL(send_replace_push, push_pool_t, push_pool, push_life);
			propose_life.reset(new msgpack::zone());
			push_life.reset(new msgpack::zone());
			pool_size = 0;
			pool_num  = 0;
		}

	}

	FLUSH_POOL(send_replace_propose, propose_pool_t, propose_pool, propose_life);
	FLUSH_POOL(send_replace_push, push_pool_t, push_pool, push_life);
}

skip_replace:
	pthread_scoped_lock relk(m_replacing_mutex);
	m_replacing.push_returned(replace_time);  // replace_copy
	if(m_replacing.is_finished(replace_time)) {
		finish_replace_copy(replace_time, relk);
	}
}

void Server::send_replace_propose(const address& node,
		const protocol::type::ReplacePropose& req,
		shared_zone& life, ClockTime replace_time)
try {
	RetryReplacePropose* retry = life->allocate<RetryReplacePropose>(req);

	retry->set_callback( BIND_RESPONSE(ResReplacePropose, retry, replace_time) );

	pthread_scoped_lock relk(m_replacing_mutex);
	m_replacing.proposed(replace_time);

	retry->call(get_node(node), life, 80);  // FIXME

} catch (std::exception& e) {
	LOG_WARN("replace propose failed: ",e.what());
}

void Server::send_replace_push(const address& node,
		const protocol::type::ReplacePush& req,
		shared_zone& life, ClockTime replace_time)
try {
	RetryReplacePush* retry = life->allocate<RetryReplacePush>(req);

	retry->set_callback( BIND_RESPONSE(ResReplacePush, retry, replace_time) );

	pthread_scoped_lock relk(m_replacing_mutex);
	m_replacing.pushed(replace_time);

	retry->call(get_node(node), life, 80);  // FIXME

} catch (std::exception& e) {
	LOG_WARN("replace push failed: ",e.what());
}


CLUSTER_FUNC(ReplacePropose, from, response, z, param)
try {
//	pthread_scoped_rdlock whlk(m_whs_mutex);
//
//	if(m_whs.empty()) {
//		//throw std::runtime_error("server not ready");
//		// don't send response if server not ready.
//		// this makes sender be timeout and it will retry
//		// after several seconds.
//		return;
//	}
//
//	// FIXME ReplacePropose may go ahead of ReplaceCopyStart
//	//       This node may have old hash space while proposer have new one
//	//if(!test_replicator_assign(m_whs, key.hash(), addr())) {
//	//	// ignore obsolete hash space error
//	//	response.null();
//	//	return;
//	//}
//
//	whlk.unlock();

	protocol::type::ReplacePropose::request request;
	pthread_scoped_rdlock dblk(m_db.mutex());

	for(uint32_t i=0; i < param.size(); ++i) {
		protocol::type::ReplacePropose::value_type& val(param[i]);
		protocol::type::DBKey key(val.dbkey());

		uint64_t clocktime;
		bool stored = DBFormat::get_clocktime(m_db,
				key.raw_data(), key.raw_size(), &clocktime);

		if(!stored || ClockTime(clocktime) < ClockTime(val.clocktime())) {
			// key is not stored OR stored key is old
			// require replication
			request.push_back(i);
	
		} else {
			// key is already deleted while replacing
		}
	}

	dblk.unlock();

	response.result(request, z);
}
RPC_CATCH(ReplacePropose, response)


CLUSTER_FUNC(ReplacePush, from, response, z, param)
try {
//	pthread_scoped_rdlock whlk(m_whs_mutex);
//
//	if(m_whs.empty()) {
//		//throw std::runtime_error("server not ready");
//		// don't send response if server is not ready.
//		// this makes sender timeout and it will retry
//		// after several seconds. see Server::ResReplacePropose.
//		return;
//	}
//
//	// Note: ReplacePush may go ahead of ReplaceCopyStart
//	//       This node may have old hash space while proposer have new one
//	//if(!test_replicator_assign(m_whs, key.hash(), addr())) {
//	//	// ignore obsolete hash space error
//	//	response.null();
//	//	return;
//	//}
//
//	whlk.unlock();

	response.result(true);  // FIXME

	pthread_scoped_wrlock dblk(m_db.mutex());

	for(protocol::type::ReplacePush::iterator it(param.begin());
			it != param.end(); ++it) {
		protocol::type::DBKey key(it->dbkey());
		protocol::type::DBValue val(it->dbval());

		bool success = m_db.setkeep(
				key.raw_data(), key.raw_size(),
				val.raw_data(), val.raw_size());
		if(success) { continue; }  // key is not stored

		uint64_t clocktime = 0;
		bool stored = DBFormat::get_clocktime(m_db,
				key.raw_data(), key.raw_size(), &clocktime);
	
		if(!stored || ClockTime(clocktime) < ClockTime(val.clocktime())) {
			// stored key is old
			m_db.set(key.raw_data(), key.raw_size(),
					 val.raw_data(), val.raw_size());
	
		} else {
			// key is already deleted while replacing
			// do nothing
		}
	}

	dblk.unlock();
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

	if(!res.is_nil() && !SESSION_IS_ACTIVE(from)) { goto skip_push; }

	try {
		typedef protocol::type::ReplacePropose::request request_t;
		request_t st(res.as<request_t>());

		if(st.empty()) { goto skip_push; }

		address addr = mp::static_pointer_cast<rpc::node>(from)->addr();

		// life is shared by multiple threads but msgpack::zone::allocate
		// is not thread-safe.
		shared_zone nlife(new msgpack::zone());
		protocol::type::ReplacePush pool;
		size_t pool_size = 0;

		protocol::type::ReplacePropose& pr(retry->param());
		pthread_scoped_rdlock dblk(m_db.mutex());

		for(request_t::iterator it(st.begin()); it != st.end(); ++it) {

			if(pr.size() < *it) { continue; }
			protocol::type::DBKey key(pr[*it].dbkey());

			uint32_t raw_vallen;
			const char* raw_val = m_db.get(key.raw_data(), key.raw_size(),
					&raw_vallen, *nlife);

			if(!raw_val || raw_vallen < DBFormat::VALUE_META_SIZE) {
				continue;
			}

			char* nraw_key = (char*)nlife->malloc(key.raw_size());
			memcpy(nraw_key, key.raw_data(), key.raw_size());

			pool.push_back( protocol::type::ReplacePushElement(
						nraw_key, key.raw_size(),
						raw_val, raw_vallen) );

			if(pool_size/1024/1024 > m_cfg_replace_pool_size) {
				send_replace_push(addr, pool, nlife, replace_time);
				nlife.reset(new msgpack::zone());
				pool_size = 0;
			}
		}
		send_replace_push(addr, pool, nlife, replace_time);
		pool_size = 0;

	} catch (std::exception& e) {
		// FIXME
		LOG_ERROR("res replace propose process failed: ",e.what());
	} catch (...) {
		LOG_ERROR("res replace propose process failed: unknown error");
	}

skip_push:
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
	LOG_INFO("finish replace copy for time(",replace_time.get(),")");

	shared_zone nullz;
	protocol::type::ReplaceCopyEnd arg(replace_time.get(), m_clock.get_incr());

	address addr;
	//{
	//	pthread_scoped_lock relk(m_replacing_mutex);
		addr = m_replacing.mgr_addr();
		m_replacing.invalidate();
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

