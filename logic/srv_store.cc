#include "logic/srv_impl.h"


#define EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE(EXCLUDE, HS, HASH, NODE, CODE) \
	EACH_ASSIGN(HS, HASH, _real_, \
			if(_real_.addr() != EXCLUDE && _real_.is_active()) { \
				shared_node NODE(get_node(_real_.addr())); \
				CODE; \
			})

namespace kumo {


void Server::check_replicator_assign(HashSpace& hs, uint64_t h)
{
	if(hs.empty()) {
		throw std::runtime_error("server not ready");
	}
	EACH_ASSIGN(hs, h, r,
			if(r.is_active()) {  // don't write to fault node
				if(r.addr() == addr()) return;
			})
	throw std::runtime_error("obsolete hash space");
}

void Server::check_coordinator_assign(HashSpace& hs, uint64_t h)
{
	if(hs.empty()) {
		throw std::runtime_error("server not ready");
	}
	EACH_ASSIGN(hs, h, r,
			if(r.is_active()) {  // don't write to fault node
				if(r.addr() != addr())
					throw std::runtime_error("obsolete hash space");
				else
					return;
			})
}


RPC_FUNC(Get, from, response, z, param)
try {
	protocol::type::DBKey key(param.dbkey());
	LOG_DEBUG("Get '",
			/*std::string(key.data(),key.size()),*/"' with hash ",
			key.hash());

	{
		pthread_scoped_rdlock rhlk(m_rhs_mutex);
		check_replicator_assign(m_rhs, key.hash());
	}

	uint32_t raw_vallen;
	const char* raw_val;
	{
		pthread_scoped_rdlock dblk(m_db.mutex());
		raw_val = m_db.get(key.raw_data(), key.raw_size(),
				&raw_vallen, *z);
	}

	if(raw_val && raw_vallen >= DBFormat::VALUE_META_SIZE) {
		LOG_DEBUG("key found");
		msgpack::type::raw_ref res(raw_val, raw_vallen);
		response.result(res, z);

	} else {
		LOG_DEBUG("key not found");
		response.null();
	}

	++m_stat_num_get;
}
RPC_CATCH(Get, response)


RPC_FUNC(Set, from, response, z, param)
try {
	protocol::type::DBKey key(param.dbkey());
	protocol::type::DBValue val(param.dbval());
	LOG_DEBUG("Set '",
			/*std::string(key.data(),key.size()),*/"' => '",
			/*std::string(val.data(),val.size()),*/"' with hash ",
			key.hash(),", with meta ",val.meta());

	pthread_scoped_rdlock whlk(m_whs_mutex);
	check_coordinator_assign(m_whs, key.hash());

	unsigned int copy_required = 0;
	shared_node repto[NUM_REPLICATION];

	EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE(addr(),
			m_whs, key.hash(), n, {
				repto[copy_required] = n;
				++copy_required;
			})
	whlk.unlock();

	ClockTime ct(m_clock.now_incr());

	val.raw_set_clocktime(ct.get());
	{
		pthread_scoped_wrlock dblk(m_db.mutex());
		m_db.set(key.raw_data(), key.raw_size(),
				 val.raw_data(), val.raw_size());
	}

	LOG_DEBUG("set copy required: ", copy_required);
	if(copy_required == 0 || param.is_async()) {
		response.result( msgpack::type::tuple<uint64_t>(ct.get()) );
	}

	// Replication
	RetryReplicateSet* retry = z->allocate<RetryReplicateSet>(
			protocol::type::ReplicateSet(
				key.raw_data(), key.raw_size(),
				val.raw_data(), val.raw_size(),
				ct.clock().get())
			);

	volatile unsigned int* pcr =
		(volatile unsigned int*)z->malloc(sizeof(volatile unsigned int));
	if(param.is_async()) { *pcr = 0; }
	else { *pcr = copy_required; }

	using namespace mp::placeholders;
	retry->set_callback( BIND_RESPONSE(ResReplicateSet,
			retry,
			pcr,
			response, ct.get()) );

	SHARED_ZONE(life, z);
	for(unsigned int i=0; i < copy_required; ++i) {
		retry->call(repto[i], life, 10);
	}

	++m_stat_num_set;
}
RPC_CATCH(Set, response)


RPC_FUNC(Delete, from, response, z, param)
try {
	protocol::type::DBKey key(param.dbkey());
	LOG_DEBUG("Delete '",
			std::string(key.data(),key.size()),"' with hash",
			key.hash());

	pthread_scoped_rdlock whlk(m_whs_mutex);
	check_coordinator_assign(m_whs, key.hash());

	unsigned int copy_required = 0;
	shared_node repto[NUM_REPLICATION];

	EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE(addr(),
			m_whs, key.hash(), n, {
				repto[copy_required] = n;
				++copy_required;
			})
	whlk.unlock();

	ClockTime ct(m_clock.now_incr());

	bool deleted;
	{
		pthread_scoped_wrlock dblk(m_db.mutex());
		deleted = m_db.del(key.raw_data(), key.raw_size());
	}
	if(!deleted) {
		response.result(false);
		// the key is not stored
		return;
	}

	LOG_DEBUG("delete copy required: ", copy_required);
	if(copy_required == 0 || param.is_async()) {
		response.result(true);
	}

	// Replication
	RetryReplicateDelete* retry = z->allocate<RetryReplicateDelete>(
			protocol::type::ReplicateDelete(
				key.raw_data(), key.raw_size(),
				ct.get(), m_clock.get_incr())
			);

	volatile unsigned int* pcr =
		(volatile unsigned int*)z->malloc(sizeof(volatile unsigned int));
	if(param.is_async()) { *pcr = 0; }
	else { *pcr = copy_required; }

	using namespace mp::placeholders;
	retry->set_callback( BIND_RESPONSE(ResReplicateDelete,
				retry,
				pcr,
				response) );
	
	SHARED_ZONE(life, z);
	for(unsigned int i=0; i < copy_required; ++i) {
		retry->call(repto[i], life, 10);
	}

	++m_stat_num_delete;
}
RPC_CATCH(Delete, response)


RPC_REPLY(ResReplicateSet, from, res, err, life,
		RetryReplicateSet* retry,
		volatile unsigned int* copy_required,
		rpc::weak_responder response, uint64_t clocktime)
{
	LOG_DEBUG("ResReplicateSet ",res,",",err," remain:",*copy_required);
	// retry if failed
	if(!err.is_nil()) {
		if(SESSION_IS_ACTIVE(from)) {
			// FIXME delayed retry?
			if(retry->retry_incr(m_cfg_replicate_set_retry_num)) {
				retry->call(from, life);
				LOG_WARN("ReplicateSet error: ",err,", retry ",retry->num_retried());
				return;
			}
		}
		response.null();
		MLOGPACK("ers",1, "Replicate set failed",
				"msg",std::string("ReplicateSet failed"),
				"key",msgtype::raw_ref(
					retry->param().dbkey().data(),
					retry->param().dbkey().size()),
				"val",msgtype::raw_ref(
					retry->param().dbval().data(),
					retry->param().dbval().size()));
		LOG_ERROR("ReplicateSet failed: ",err);
		return;
	}

	LOG_DEBUG("ReplicateSet succeeded");

	if(__sync_sub_and_fetch(copy_required, 1) == 0) {
		response.result( msgpack::type::tuple<uint64_t>(clocktime) );
	}
}

RPC_REPLY(ResReplicateDelete, from, res, err, life,
		RetryReplicateDelete* retry,
		volatile unsigned int* copy_required,
		rpc::weak_responder response)
{
	// retry if failed
	if(!err.is_nil()) {
		if(SESSION_IS_ACTIVE(from)) {
			// FIXME delayed retry?
			if(retry->retry_incr(m_cfg_replicate_delete_retry_num)) {
				retry->call(from, life);
				LOG_WARN("ReplicateDelete error: ",err,", retry ",retry->num_retried());
				return;
			}
		}
		response.null();
		MLOGPACK("erd",1, "Replicate delete failed",
				"msg",std::string("ReplicateDelete failed"),
				"key",msgtype::raw_ref(
					retry->param().dbkey().data(),
					retry->param().dbkey().size()));
		LOG_ERROR("ReplicateDelete failed: ",err);
		return;
	}

	LOG_DEBUG("ReplicateDelete succeeded");

	if(__sync_sub_and_fetch(copy_required, 1) == 0) {
		response.result(true);
	}
}


CLUSTER_FUNC(ReplicateSet, from, response, z, param)
try {
	protocol::type::DBKey key = param.dbkey();
	protocol::type::DBValue val = param.dbval();
	LOG_TRACE("ReplicateSet");

	{
		pthread_scoped_rdlock whlk(m_whs_mutex);
		check_replicator_assign(m_whs, key.hash());
	}

	m_clock.update(param.clock());

	pthread_scoped_wrlock dblk(m_db.mutex());

	bool success = m_db.setkeep(
			key.raw_data(), key.raw_size(),
			val.raw_data(), val.raw_size());
	if(success) {
		// key is not stored
		response.result(true);
		return;
	}

	uint64_t clocktime = 0;
	bool stored = DBFormat::get_clocktime(m_db,
			key.raw_data(), key.raw_size(), &clocktime);

	if(!stored || ClockTime(clocktime) <= ClockTime(val.clocktime())) {
		// stored key is old
		m_db.set(key.raw_data(), key.raw_size(),
				 val.raw_data(), val.raw_size());
		dblk.unlock();
		response.result(true);

	} else {
		// key is overwritten while replicating
		// do nothing
		dblk.unlock();
		response.result(false);
	}
}
RPC_CATCH(ReplicateSet, response)

CLUSTER_FUNC(ReplicateDelete, from, response, z, param)
try {
	protocol::type::DBKey key = param.dbkey();
	LOG_TRACE("ReplicateDelete");

	// FIXME check write-hash-space assignment?
	//{
	//	pthread_scoped_rdlock whlk(m_whs_mutex);
	//	check_replicator_assign(m_whs, key.hash());
	//}

	m_clock.update(param.clock());

	pthread_scoped_wrlock dblk(m_db.mutex());
	uint64_t clocktime;
	bool stored = DBFormat::get_clocktime(m_db,
			key.raw_data(), key.raw_size(), &clocktime);

	if(!stored) {
		// key is not stored
		// do nothing
		dblk.unlock();
		response.result(true);

	} else if(ClockTime(clocktime) <= ClockTime(param.clocktime())) {
		// stored key is old
		m_db.del(key.raw_data(), key.raw_size());
		dblk.unlock();
		response.result(true);

	} else {
		// key is already deleted while replicating
		// do nothing
		dblk.unlock();
		LOG_TRACE("obsolete replicate push");
		response.result(false);
	}
}
RPC_CATCH(ReplicateDelete, response)


}  // namespace kumo

