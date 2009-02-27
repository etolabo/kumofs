#include "logic/srv_impl.h"


#define EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_ONE(EXCLUDE, HS, HASH, NODE, CODE) \
	EACH_ASSIGN(HS, HASH, _real_, \
			if(_real_.addr() != EXCLUDE && _real_.is_active()) { \
				shared_node NODE(get_node(_real_.addr())); \
				CODE; \
			})

#define EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_N(EXCLUDES, EXCLUDES_NUM, HS, HASH, NODE, CODE) \
	EACH_ASSIGN(HS, HASH, _real_, \
			if(_real_.is_active()) { \
				bool exclude = false; \
				for(unsigned int i=0; i < (EXCLUDES_NUM); ++i) { \
					if(_real_.addr() == EXCLUDES[i]) { \
						exclude = true; \
						break; \
					} \
				} \
				if(!exclude) { \
					shared_node NODE(get_node(_real_.addr())); \
					CODE; \
				} \
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
	const char* raw_val = m_db.get(
			key.raw_data(), key.raw_size(),
			&raw_vallen, z);

	if(val) {
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


bool Server::SetByRhsWhs(weak_responder response, auto_zone& z,
		protocol::type::DBKey& key, protocol::type::DBValue& val,
		bool is_async)
{
	unsigned int rrep_num = 0;
	unsigned int wrep_num = 0;
	shared_node rrepto[NUM_REPLICATION];
	shared_node wrepto[NUM_REPLICATION];

	{
		pthread_scoped_rdlock whlk(m_whs_mutex);
		check_coordinator_assign(m_whs, key.hash());

		pthread_scoped_rdlock rhlk(m_rhs_mutex);

		if(m_whs.clocktime() == m_rhs.clocktime()) {
			return false;
		}

		address wrep_addrs[NUM_REPLICATION+1];

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_ONE(addr(),
				m_whs, key.hash(), n, {
					wrepto[wrep_num] = n;
					wrep_addrs[wrep_num] = n->addr();
					++wrep_num;
				})

		whlk.unlock();

		wrep_addrs[wrep_num] = addr();  // exclude self

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_N(wrep_addrs, wrep_num+1,
				m_rhs, key.hash(), n, {
					rrepto[rrep_num++] = n;
				})
	}

	ClockTime ct(m_clock.now_incr());
	val.raw_set_clocktime(ct.get());

	volatile unsigned int* pcr =
		(volatile unsigned int*)z->malloc(sizeof(volatile unsigned int));
	if(is_async) { *pcr = 0; }
	else { *pcr = wrep_num + rrep_num; }

	using namespace mp::placeholders;

	// rhs Replication
	RetryReplicateSet* rretry = z->allocate<RetryReplicateSet>(
			protocol::type::ReplicateSet(
				key.raw_data(), key.raw_size(),
				val.raw_data(), val.raw_size(),
				ct.clock().get(), true)
			);
	rretry->set_callback( BIND_RESPONSE(ResReplicateSet,
			rretry,
			pcr,
			response, ct.get()) );

	// whs Replication
	RetryReplicateSet* wretry = z->allocate<RetryReplicateSet>(
			protocol::type::ReplicateSet(
				key.raw_data(), key.raw_size(),
				val.raw_data(), val.raw_size(),
				ct.clock().get(), false)
			);
	wretry->set_callback( BIND_RESPONSE(ResReplicateSet,
			wretry,
			pcr,
			response, ct.get()) );

	SHARED_ZONE(life, z);

	for(unsigned int i=0; i < rrep_num; ++i) {
		rretry->call(rrepto[i], life, 10);
	}

	for(unsigned int i=0; i < wrep_num; ++i) {
		wretry->call(wrepto[i], life, 10);
	}

	m_db.set(
			key.raw_data(), key.raw_size(),
			val.raw_data(), val.raw_size());

	LOG_DEBUG("set copy required: ", wrep_num+rrep_num);
	if((wrep_num == 0 && rrep_num == 0) || is_async) {
		response.result( msgpack::type::tuple<uint64_t>(ct.get()) );
	}

	return true;
}

void Server::SetByWhs(weak_responder response, auto_zone& z,
		protocol::type::DBKey& key, protocol::type::DBValue& val,
		bool is_async)
{
	unsigned int wrep_num = 0;
	shared_node wrepto[NUM_REPLICATION];

	{
		pthread_scoped_rdlock whlk(m_whs_mutex);
		check_coordinator_assign(m_whs, key.hash());

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_ONE(addr(),
				m_whs, key.hash(), n, {
					wrepto[wrep_num++] = n;
				})
	}

	ClockTime ct(m_clock.now_incr());
	val.raw_set_clocktime(ct.get());

	volatile unsigned int* pcr =
		(volatile unsigned int*)z->malloc(sizeof(volatile unsigned int));
	if(is_async) { *pcr = 0; }
	else { *pcr = wrep_num; }

	using namespace mp::placeholders;

	// whs Replication
	RetryReplicateSet* retry = z->allocate<RetryReplicateSet>(
			protocol::type::ReplicateSet(
				key.raw_data(), key.raw_size(),
				val.raw_data(), val.raw_size(),
				ct.clock().get(), false)
			);
	retry->set_callback( BIND_RESPONSE(ResReplicateSet,
			retry,
			pcr,
			response, ct.get()) );
	
	SHARED_ZONE(life, z);
	for(unsigned int i=0; i < wrep_num; ++i) {
		retry->call(wrepto[i], life, 10);
	}

	m_db.set(
			key.raw_data(), key.raw_size(),
			val.raw_data(), val.raw_size());

	LOG_DEBUG("set copy required: ", wrep_num);
	if(wrep_num == 0 || is_async) {
		response.result( msgpack::type::tuple<uint64_t>(ct.get()) );
	}
}

RPC_FUNC(Set, from, response, z, param)
try {
	protocol::type::DBKey key(param.dbkey());
	protocol::type::DBValue val(param.dbval());
	LOG_DEBUG("Set '",
			/*std::string(key.data(),key.size()),*/"' => '",
			/*std::string(val.data(),val.size()),*/"' with hash ",
			key.hash(),", with meta ",val.meta());

	if(m_whs.clocktime() != m_rhs.clocktime()) {
		if( !SetByRhsWhs(response, z, key, val, param.is_async()) ) {
			SetByWhs(response, z, key, val, param.is_async());
		}
	} else {
		SetByWhs(response, z, key, val, param.is_async());
	}

	++m_stat_num_set;
}
RPC_CATCH(Set, response)



bool Server::DeleteByRhsWhs(weak_responder response, auto_zone& z,
		protocol::type::DBKey& key,
		bool is_async)
{
	unsigned int rrep_num = 0;
	unsigned int wrep_num = 0;
	shared_node rrepto[NUM_REPLICATION];
	shared_node wrepto[NUM_REPLICATION];

	{
		pthread_scoped_rdlock whlk(m_whs_mutex);
		check_coordinator_assign(m_whs, key.hash());

		pthread_scoped_rdlock rhlk(m_rhs_mutex);

		if(m_whs.clocktime() == m_rhs.clocktime()) {
			return false;
		}

		address wrep_addrs[NUM_REPLICATION+1];

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_ONE(addr(),
				m_whs, key.hash(), n, {
					wrepto[wrep_num] = n;
					wrep_addrs[wrep_num] = n->addr();
					++wrep_num;
				})

		whlk.unlock();

		wrep_addrs[wrep_num] = addr();  // exclude self

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_N(wrep_addrs, wrep_num+1,
				m_rhs, key.hash(), n, {
					rrepto[rrep_num++] = n;
				})
	}

	bool deleted = m_db.del(key.raw_data(), key.raw_size());
	if(!deleted) {
		//response.result(false);
		// the key is not stored
		//return true;
		wrep_num = 0;
	}

	ClockTime ct(m_clock.now_incr());

	LOG_DEBUG("delete copy required: ", wrep_num+rrep_num);
	if((wrep_num == 0 && rrep_num == 0) || is_async) {
		response.result(true);
	}

	volatile unsigned int* pcr =
		(volatile unsigned int*)z->malloc(sizeof(volatile unsigned int));
	if(is_async) { *pcr = 0; }
	else { *pcr = wrep_num + rrep_num; }

	using namespace mp::placeholders;

	// rhs Replication
	RetryReplicateDelete* rretry = z->allocate<RetryReplicateDelete>(
			protocol::type::ReplicateDelete(
				key.raw_data(), key.raw_size(),
				ct.get(), ct.clock().get(), true)
			);
	rretry->set_callback( BIND_RESPONSE(ResReplicateDelete,
			rretry,
			pcr,
			response, deleted) );

	// whs Replication
	RetryReplicateDelete* wretry = z->allocate<RetryReplicateDelete>(
			protocol::type::ReplicateDelete(
				key.raw_data(), key.raw_size(),
				ct.get(), ct.clock().get(), false)
			);
	wretry->set_callback( BIND_RESPONSE(ResReplicateDelete,
			wretry,
			pcr,
			response, deleted) );

	SHARED_ZONE(life, z);

	for(unsigned int i=0; i < rrep_num; ++i) {
		rretry->call(rrepto[i], life, 10);
	}

	for(unsigned int i=0; i < wrep_num; ++i) {
		wretry->call(wrepto[i], life, 10);
	}

	return true;
}

void Server::DeleteByWhs(weak_responder response, auto_zone& z,
		protocol::type::DBKey& key,
		bool is_async)
{
	unsigned int wrep_num = 0;
	shared_node wrepto[NUM_REPLICATION];

	{
		pthread_scoped_rdlock whlk(m_whs_mutex);
		check_coordinator_assign(m_whs, key.hash());

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_ONE(addr(),
				m_whs, key.hash(), n, {
					wrepto[wrep_num++] = n;
				})
	}

	bool deleted = m_db.del(key.raw_data(), key.raw_size());
	if(!deleted) {
		response.result(false);
		// the key is not stored
		return;
	}

	ClockTime ct(m_clock.now_incr());

	LOG_DEBUG("delete copy required: ", wrep_num);
	if(wrep_num == 0 || is_async) {
		response.result(true);
	}

	volatile unsigned int* pcr =
		(volatile unsigned int*)z->malloc(sizeof(volatile unsigned int));
	if(is_async) { *pcr = 0; }
	else { *pcr = wrep_num; }

	using namespace mp::placeholders;

	// whs Replication
	RetryReplicateDelete* retry = z->allocate<RetryReplicateDelete>(
			protocol::type::ReplicateDelete(
				key.raw_data(), key.raw_size(),
				ct.get(), ct.clock().get(), false)
			);
	retry->set_callback( BIND_RESPONSE(ResReplicateDelete,
			retry,
			pcr,
			response, deleted) );
	
	SHARED_ZONE(life, z);
	for(unsigned int i=0; i < wrep_num; ++i) {
		retry->call(wrepto[i], life, 10);
	}
}

RPC_FUNC(Delete, from, response, z, param)
try {
	protocol::type::DBKey key(param.dbkey());
	LOG_DEBUG("Delete '",
			std::string(key.data(),key.size()),"' with hash",
			key.hash());

	if(m_whs.clocktime() != m_rhs.clocktime()) {
		if( !DeleteByRhsWhs(response, z, key, param.is_async()) ) {
			DeleteByWhs(response, z, key, param.is_async());
		}
	} else {
		DeleteByWhs(response, z, key, param.is_async());
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
		if(!retry->param().is_rhs()) {  // FIXME ?
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
	}

	LOG_DEBUG("ReplicateSet succeeded");

	if(__sync_sub_and_fetch(copy_required, 1) == 0) {
		response.result( msgpack::type::tuple<uint64_t>(clocktime) );
	}
}

RPC_REPLY(ResReplicateDelete, from, res, err, life,
		RetryReplicateDelete* retry,
		volatile unsigned int* copy_required,
		rpc::weak_responder response, bool deleted)
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
		if(!retry->param().is_rhs()) {  // FIXME ?
			response.null();
			MLOGPACK("erd",1, "Replicate delete failed",
					"msg",std::string("ReplicateDelete failed"),
					"key",msgtype::raw_ref(
						retry->param().dbkey().data(),
						retry->param().dbkey().size()));
			LOG_ERROR("ReplicateDelete failed: ",err);
			return;
		}
	}

	LOG_DEBUG("ReplicateDelete succeeded");

	if(__sync_sub_and_fetch(copy_required, 1) == 0) {
		if(!deleted && retry->param().is_rhs() &&
				res.type == msgpack::type::BOOLEAN && res.via.boolean == true) {
			deleted = true;
		}
		response.result(deleted);
	}
}


CLUSTER_FUNC(ReplicateSet, from, response, z, param)
try {
	protocol::type::DBKey key = param.dbkey();
	protocol::type::DBValue val = param.dbval();
	LOG_TRACE("ReplicateSet");

	if(param.is_rhs()) {
		pthread_scoped_rdlock rhlk(m_rhs_mutex);
		check_replicator_assign(m_rhs, key.hash());
	} else {
		pthread_scoped_rdlock whlk(m_whs_mutex);
		check_replicator_assign(m_whs, key.hash());
	}

	m_clock.update(param.clock());

	bool updated = m_db.update(
			key.raw_data(), key.raw_size(),
			val.raw_data(), val.raw_size());

	response.result(updated);
}
RPC_CATCH(ReplicateSet, response)

CLUSTER_FUNC(ReplicateDelete, from, response, z, param)
try {
	protocol::type::DBKey key = param.dbkey();
	LOG_TRACE("ReplicateDelete");

	if(param.is_rhs()) {
		pthread_scoped_rdlock rhlk(m_rhs_mutex);
		check_replicator_assign(m_rhs, key.hash());
	} else {
		pthread_scoped_rdlock whlk(m_whs_mutex);
		check_replicator_assign(m_whs, key.hash());
	}

	m_clock.update(param.clock());

	bool deleted = m_db.del(key.raw_data(), key.raw_size(),
			param.clocktime());

	response.result(deleted);
}
RPC_CATCH(ReplicateDelete, response)


}  // namespace kumo

