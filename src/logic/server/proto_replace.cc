#include "server/framework.h"
#include "server/proto_replace.h"
#include "manager/proto_replace.h"

namespace kumo {
namespace server {


bool proto_replace::test_replicator_assign(const HashSpace& hs, uint64_t h, const address& target)
{
	EACH_ASSIGN(hs, h, r,
			if(r.is_active()) {  // don't write to fault node
				if(r.addr() == target) return true;
			})
	return false;
}


proto_replace::replace_state::replace_state() :
	m_push_waiting(0),
	m_clocktime(0) {}

proto_replace::replace_state::~replace_state() {}

inline void proto_replace::replace_state::reset(const address& mgr, ClockTime ct)
{
	m_push_waiting = 0;
	m_clocktime = ct;
	m_mgr = mgr;
}

inline void proto_replace::replace_state::pushed(ClockTime ct)
{
	if(ct == m_clocktime) { ++m_push_waiting; }
}

inline void proto_replace::replace_state::push_returned(ClockTime ct)
{
	if(ct == m_clocktime) { --m_push_waiting; }
}

inline bool proto_replace::replace_state::is_finished(ClockTime ct) const
{
	return m_clocktime == ct && m_push_waiting == 0;
}

inline void proto_replace::replace_state::invalidate()
{
	m_push_waiting = -1;
}

inline const address& proto_replace::replace_state::mgr_addr() const
{
	return m_mgr;
}


void proto_replace::replace_offer_push(ClockTime replace_time, REQUIRE_STLK)
{
	m_state.pushed(replace_time);
}

void proto_replace::replace_offer_pop(ClockTime replace_time, REQUIRE_STLK)
{
	m_state.push_returned(replace_time);
	if(m_state.is_finished(replace_time)) {
		finish_replace_copy(replace_time, stlk);
	}
}



RPC_IMPL(proto_replace, ReplaceCopyStart, req, z, response)
{
	net->clock_update(req.param().clock);

	HashSpace hs(req.param().hsseed);

	response.result(true);

	try {
		replace_copy(req.node()->addr(), hs);
	} catch (std::runtime_error& e) {
		LOG_ERROR("replace copy failed: ",e.what());
	} catch (...) {
		LOG_ERROR("replace copy failed: unknown error");
	}
}


RPC_IMPL(proto_replace, ReplaceDeleteStart, req, z, response)
{
	net->clock_update(req.param().clock);

	HashSpace hs(req.param().hsseed);

	response.result(true);

	try {
		replace_delete(req.node(), hs);
	} catch (std::runtime_error& e) {
		LOG_ERROR("replace delete failed: ",e.what());
	} catch (...) {
		LOG_ERROR("replace delete failed: unknown error");
	}
}


struct proto_replace::for_each_replace_copy {
	for_each_replace_copy(
			const address& addr,
			const HashSpace& src, const HashSpace& dst,
			proto_replace_stream::offer_storage& offer_storage,
			const addrvec_t& faults) :
		self(addr),
		srchs(src), dsths(dst),
		offer(offer_storage), fault_nodes(faults)
	{
		Sa.reserve(NUM_REPLICATION+1);
		Da.reserve(NUM_REPLICATION+1);
		current_owners.reserve(NUM_REPLICATION+1);
		newbies.reserve(NUM_REPLICATION+1);
	}

	inline void operator() (Storage::iterator& kv);

private:
	addrvec_t Sa;
	addrvec_t Da;
	addrvec_t current_owners;
	addrvec_t newbies;

	const address& self;

	const HashSpace& srchs;
	const HashSpace& dsths;

	proto_replace_stream::offer_storage& offer;
	const addrvec_t& fault_nodes;

private:
	for_each_replace_copy();
};

void proto_replace::replace_copy(const address& manager_addr, HashSpace& hs)
{
	ClockTime replace_time = hs.clocktime();

	{
		pthread_scoped_lock stlk(m_state_mutex);
		m_state.reset(manager_addr, replace_time);
		replace_offer_push(replace_time, stlk);  // replace_copy;
	}

	LOG_INFO("start replace copy for time(",replace_time.get(),")");

	pthread_scoped_wrlock whlock(share->whs_mutex());
	pthread_scoped_wrlock rhlk(share->rhs_mutex());

	HashSpace srchs(share->rhs());
	whlock.unlock();

	share->whs() = hs;
	whlock.unlock();

	HashSpace& dsths(hs);

	addrvec_t fault_nodes;
	{
		addrvec_t src_nodes;
		addrvec_t dst_nodes;
	
		srchs.get_active_nodes(src_nodes);
		dsths.get_active_nodes(dst_nodes);
	
		for(addrvec_iterator it(src_nodes.begin()); it != src_nodes.end(); ++it) {
			LOG_INFO("src active node: ",*it);
		}
		for(addrvec_iterator it(dst_nodes.begin()); it != dst_nodes.end(); ++it) {
			LOG_INFO("dst active node: ",*it);
		}
	
		if(src_nodes.empty() || dst_nodes.empty()) {
			LOG_INFO("empty hash space. skip replacing.");
			goto skip_replace;
		}
	
		std::sort(src_nodes.begin(), src_nodes.end());
		std::sort(dst_nodes.begin(), dst_nodes.end());
	
		for(addrvec_iterator it(src_nodes.begin()); it != src_nodes.end(); ++it) {
			if(!std::binary_search(dst_nodes.begin(), dst_nodes.end(), *it)) {
				fault_nodes.push_back(*it);
			}
		}
	
		for(addrvec_iterator it(fault_nodes.begin()); it != fault_nodes.end(); ++it) {
			LOG_INFO("fault node: ",*it);
		}
	
		if(std::binary_search(fault_nodes.begin(), fault_nodes.end(), net->addr())) {
			LOG_WARN("I'm marked as fault. skip replacing.");
			goto skip_replace;
		}
	}

	{
		proto_replace_stream::offer_storage offer(share->cfg_offer_tmpdir(), replace_time);

		share->db().for_each(
				for_each_replace_copy(net->addr(), srchs, dsths, offer, fault_nodes),
				net->clocktime_now());

		net->scope_proto_replace_stream().send_offer(offer, replace_time);
	}

skip_replace:
	pthread_scoped_lock stlk(m_state_mutex);
	replace_offer_pop(replace_time, stlk);  // replace_copy
}

void proto_replace::for_each_replace_copy::operator() (Storage::iterator& kv)
{
	const char* raw_key = kv.key();
	size_t raw_keylen = kv.keylen();
	const char* raw_val = kv.val();
	size_t raw_vallen = kv.vallen();

	// Note: it is done in storage wrapper.
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
	for(addrvec_iterator it(Sa.begin()); it != Sa.end(); ++it) {
		if(!std::binary_search(fault_nodes.begin(), fault_nodes.end(), *it)) {
			current_owners.push_back(*it);
		}
	}

	// FIXME 再配置中にServerがダウンしたときコピーが正常に行われないかもしれない？
	if(current_owners.empty() || current_owners.front() != self) { return; }
	//if(std::find(current_owners.begin(), current_owners.end(), self)
	//		== current_owners.end()) { return; }

	newbies.clear();
	for(addrvec_iterator it(Da.begin()); it != Da.end(); ++it) {
		if(std::find(Sa.begin(), Sa.end(), *it) == Sa.end()) {
			newbies.push_back(*it);
		}
	}

	if(newbies.empty()) { return; }

	for(addrvec_iterator it(newbies.begin()); it != newbies.end(); ++it) {
		offer.add(*it,
				raw_key, raw_keylen,
				raw_val, raw_vallen);
	}
}



void proto_replace::finish_replace_copy(ClockTime replace_time, REQUIRE_STLK)
{
	LOG_INFO("finish replace copy for time(",replace_time.get(),")");

	shared_zone nullz;
	manager::proto_replace::ReplaceCopyEnd param(
			replace_time, net->clock_incr());

	address addr;
	//{
	//	pthread_scoped_lock stlk(m_state_mutex);
		addr = m_state.mgr_addr();
		m_state.invalidate();
	//}

	using namespace mp::placeholders;
	net->get_node(addr)->call(param, nullz,
			BIND_RESPONSE(proto_replace, ReplaceCopyEnd), 10);
}

RPC_REPLY_IMPL(proto_replace, ReplaceCopyEnd, from, res, err, life)
{
	if(!err.is_nil()) { LOG_ERROR("ReplaceCopyEnd failed: ",err); }
	// FIXME retry
}


struct proto_replace::for_each_replace_delete {
	for_each_replace_delete(const HashSpace& hs, const address& addr) :
		self(addr), m_hs(hs) { }

	inline void operator() (Storage::iterator& kv);

private:
	const address& self;
	const HashSpace& m_hs;

private:
	for_each_replace_delete();
};

void proto_replace::replace_delete(shared_node& manager, HashSpace& hs)
{
	pthread_scoped_rdlock whlk(share->whs_mutex());

	pthread_scoped_wrlock rhlk(share->rhs_mutex());
	share->rhs() = share->whs();
	rhlk.unlock();

	LOG_INFO("start replace delete for time(",share->whs().clocktime().get(),")");

	if(!share->whs().empty()) {
		share->db().for_each(
				for_each_replace_delete(share->whs(), net->addr()),
				net->clocktime_now() );
	}

	shared_zone nullz;
	manager::proto_replace::ReplaceDeleteEnd param(
			share->whs().clocktime(), net->clock_incr());

	using namespace mp::placeholders;
	manager->call(param, nullz,
			BIND_RESPONSE(proto_replace, ReplaceDeleteEnd), 10);

	LOG_INFO("finish replace for time(",share->whs().clocktime().get(),")");
}

void proto_replace::for_each_replace_delete::operator() (Storage::iterator& kv)
{
	// Note: it is done in storage wrapper.
	//if(kv.keylen() < Storage::KEY_META_SIZE ||
	//		kv.vallen() < Storage::VALUE_META_SIZE) {
	//	LOG_TRACE("delete invalid key: ",kv.key());
	//	kv.del();
	//}
	uint64_t h = Storage::hash_of(kv.key());
	if(!proto_replace::test_replicator_assign(m_hs, h, self)) {
		LOG_TRACE("replace delete key: ",kv.key());
		kv.del();
	}
}

RPC_REPLY_IMPL(proto_replace, ReplaceDeleteEnd, from, res, err, life)
{
	if(!err.is_nil()) {
		LOG_ERROR("ReplaceDeleteEnd failed: ",err);
	}
	// FIXME retry
}


}  // namespace server
}  // namespace kumo

