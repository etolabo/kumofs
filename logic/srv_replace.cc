#include "logic/srv_impl.h"
#include <algorithm>

namespace kumo {


bool Server::test_replicator_assign(const HashSpace& hs, uint64_t h, const address& target)
{
	EACH_ASSIGN(hs, h, r,
			if(r.is_active()) {  // don't write to fault node
				if(r.addr() == target) return true;
			})
	return false;
}


Server::ReplaceContext::ReplaceContext() :
	m_push_waiting(0),
	m_clocktime(0) {}

Server::ReplaceContext::~ReplaceContext() {}

inline void Server::ReplaceContext::reset(const address& mgr, ClockTime ct)
{
	m_push_waiting = 0;
	m_clocktime = ct;
	m_mgr = mgr;
}

inline void Server::ReplaceContext::pushed(ClockTime ct)
	{ if(ct == m_clocktime) { ++m_push_waiting; } }
inline void Server::ReplaceContext::push_returned(ClockTime ct)
	{ if(ct == m_clocktime) { --m_push_waiting; } }

inline bool Server::ReplaceContext::is_finished(ClockTime ct) const
	{ return m_clocktime == ct && m_push_waiting == 0; }

inline void Server::ReplaceContext::invalidate()
	{ m_push_waiting = -1; }

inline const address& Server::ReplaceContext::mgr_addr() const
	{ return m_mgr; }

void Server::replace_offer_start(ClockTime replace_time, REQUIRE_RELK)
{
	m_replacing.pushed(replace_time);
}

void Server::replace_offer_finished(ClockTime replace_time, REQUIRE_RELK)
{
	m_replacing.push_returned(replace_time);
	if(m_replacing.is_finished(replace_time)) {
		finish_replace_copy(replace_time, relk);
	}
}


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
template <typename OfferStorageMap, typename addrs_t, typename addrs_it>
struct for_each_replace_copy {
	for_each_replace_copy(
			const address& addr,
			const HashSpace& src, const HashSpace& dst,
			OfferStorageMap& accumulator, const addrs_t& faults) :
		self(addr),
		srchs(src), dsths(dst),
		offer(accumulator), fault_nodes(faults)
	{
		Sa.reserve(NUM_REPLICATION+1);
		Da.reserve(NUM_REPLICATION+1);
		current_owners.reserve(NUM_REPLICATION+1);
		newbies.reserve(NUM_REPLICATION+1);
	}

	void operator() (Storage::iterator& kv)
	{
		const char* raw_key = kv.key();
		size_t raw_keylen = kv.keylen();
		const char* raw_val = kv.val();
		size_t raw_vallen = kv.vallen();

		// FIXME do it in storage module.
		//if(raw_vallen < Storage::VALUE_META_SIZE) { return; }
		//if(raw_keylen < Storage::KEY_META_SIZE) { return; }

		uint64_t h = Storage::hash_of(kv.key());

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

		// FIXME 再配置中にServerがダウンしたときコピーが正常に行われないかもしれない
		//if(current_owners.empty() || current_owners.front() != self) { return; }
		if(std::find(current_owners.begin(), current_owners.end(), self)
				== current_owners.end()) { return; }

		newbies.clear();
		for(addrs_it it(Da.begin()); it != Da.end(); ++it) {
			if(std::find(Sa.begin(), Sa.end(), *it) == Sa.end()) {
				newbies.push_back(*it);
			}
		}

		if(newbies.empty()) { return; }

		for(addrs_it it(newbies.begin()); it != newbies.end(); ++it) {
			offer.add(*it,
					raw_key, raw_keylen,
					raw_val, raw_vallen);
		}
	}

private:
	addrs_t Sa;
	addrs_t Da;
	addrs_t current_owners;
	addrs_t newbies;

	const address& self;

	const HashSpace& srchs;
	const HashSpace& dsths;

	OfferStorageMap& offer;
	const addrs_t& fault_nodes;

	for_each_replace_copy();
};
}  // noname namespace

void Server::replace_copy(const address& manager_addr, HashSpace& hs)
{
	ClockTime replace_time = hs.clocktime();

	{
		pthread_scoped_lock relk(m_replacing_mutex);
		m_replacing.reset(manager_addr, replace_time);
		replace_offer_start(replace_time, relk);  // replace_copy;
	}

	LOG_INFO("start replace copy for time(",replace_time.get(),")");

	pthread_scoped_wrlock whlock(m_whs_mutex);
	pthread_scoped_wrlock rhlk(m_rhs_mutex);

	HashSpace srchs(m_rhs);
	whlock.unlock();

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
		OfferStorageMap offer(m_cfg_offer_tmpdir, replace_time);

		m_db.for_each(
				for_each_replace_copy<OfferStorageMap, addrs_t, addrs_it>(
					addr(), srchs, dsths, offer, fault_nodes) );

		send_offer(offer, replace_time);
	}

skip_replace:
	pthread_scoped_lock relk(m_replacing_mutex);
	replace_offer_finished(replace_time, relk);  // replace_copy
}


void Server::send_offer(OfferStorageMap& offer, ClockTime replace_time)
{
	pthread_scoped_lock oflk(m_offer_map_mutex);
	offer.commit(&m_offer_map);

	pthread_scoped_lock relk(m_replacing_mutex);

	for(SharedOfferMap::iterator it(m_offer_map.begin()),
			it_end(m_offer_map.end()); it != it_end; ++it) {
		const address& addr( (*it)->addr() );
		LOG_DEBUG("send offer to ",(*it)->addr());
		shared_zone nullz;
		protocol::type::ReplaceOffer arg(m_stream_addr.port());
		using namespace mp::placeholders;
		get_node(addr)->call(
				protocol::ReplaceOffer, arg, nullz,
				BIND_RESPONSE(ResReplaceOffer, replace_time, addr), 160);  // FIXME 160

		replace_offer_start(replace_time, relk);
	}
}

CLUSTER_FUNC(ReplaceOffer, from, response, z, param)
try {
	address stream_addr = from->addr();
	stream_addr.set_port(param.port());
	char addrbuf[stream_addr.addrlen()];
	stream_addr.getaddr((sockaddr*)addrbuf);

	using namespace mp::placeholders;
	m_stream_core->connect(
			PF_INET, SOCK_STREAM, 0,
			(sockaddr*)addrbuf, sizeof(addrbuf),
			m_connect_timeout_msec,
			mp::bind(&Server::stream_connected, this, _1, _2));
	// Note: don't return any result
	LOG_TRACE("connect replace offer to ",from->addr()," with stream port ",param.port());
}
RPC_CATCH(ReplaceDeleteStart, response)

RPC_REPLY(ResReplaceOffer, from, res, err, life,
		ClockTime replace_time, address addr)
{
	LOG_TRACE("ResReplaceOffer from ",addr," res:",res," err:",err);
	// Note: this request always timed out
	pthread_scoped_lock oflk(m_offer_map_mutex);
	SharedOfferMap::iterator it = find_offer_map(m_offer_map, addr);
	if(it == m_offer_map.end()) { return; }
	m_offer_map.erase(it);
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


namespace {
template <bool (*test_replicator_assign)(const HashSpace& hs, uint64_t h, const address& target)>
struct for_each_replace_delete {
	for_each_replace_delete(const HashSpace& whs, const address& addr) :
		self(addr), m_whs(whs) { }

	void operator() (Storage::iterator& kv)
	{
		// FIXME do it in storage module.
		//if(kv.keylen() < Storage::KEY_META_SIZE ||
		//		kv.vallen() < Storage::VALUE_META_SIZE) {
		//	LOG_TRACE("delete invalid key: ",kv.key());
		//	kv.del();
		//}
		uint64_t h = Storage::hash_of(kv.key());
		if(!test_replicator_assign(m_whs, h, self)) {
			LOG_TRACE("replace delete key: ",kv.key());
			kv.del();
		}
	}

private:
	const address& self;
	const HashSpace& m_whs;
	for_each_replace_delete();
};
}  // noname namespace

void Server::replace_delete(shared_node& manager, HashSpace& hs)
{
	pthread_scoped_rdlock whlk(m_whs_mutex);

	pthread_scoped_wrlock rhlk(m_rhs_mutex);
	m_rhs = m_whs;
	rhlk.unlock();

	LOG_INFO("start replace delete for time(",m_whs.clocktime().get(),")");

	if(!m_whs.empty()) {
		m_db.for_each(
				for_each_replace_delete<&Server::test_replicator_assign>(
					m_whs, addr()) );
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

