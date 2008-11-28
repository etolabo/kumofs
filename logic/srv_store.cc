#include "logic/srv_impl.h"


#define EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE(EXCLUDE, HS, HASH, NODE, CODE) \
	EACH_ASSIGN(HS, HASH, _real_, \
			if(_real_.addr() != EXCLUDE && _real_.is_active()) { \
				shared_node NODE(get_node(_real_.addr())); \
				CODE; \
			})

namespace kumo {


void Server::check_replicator_assign(HashSpace& hs, const char* key, uint32_t keylen)
{
	if(hs.empty()) {
		throw std::runtime_error("server not ready");
	}
	uint64_t h = HashSpace::hash(key, keylen);
	EACH_ASSIGN(hs, h, r,
			if(r.is_active()) {  // don't write to fault node
				if(r.addr() == addr()) return;
			})
	throw std::runtime_error("obsolete hash space");
}

void Server::check_coordinator_assign(HashSpace& hs, const char* key, uint32_t keylen)
{
	if(hs.empty()) {
		throw std::runtime_error("server not ready");
	}
	uint64_t h = HashSpace::hash(key, keylen);
	EACH_ASSIGN(hs, h, r,
			if(r.is_active()) {  // don't write to fault node
				if(r.addr() != addr())
					throw std::runtime_error("obsolete hash space");
				else
					return;
			})
}


RPC_FUNC(Get, from, response, life, param)
try {
	LOG_DEBUG("Get '",std::string(param.key(),param.keylen()),"'");

	check_replicator_assign(m_rhs, param.key(), param.keylen());

	uint32_t meta_vallen;
	const char* meta_val = m_db.get(param.key(), param.keylen(),
			&meta_vallen, *life);

	if(meta_val && meta_vallen >= DBFormat::LEADING_METADATA_SIZE) {
		LOG_DEBUG("key found");

		DBFormat form(meta_val, meta_vallen);
		msgpack::type::tuple<msgpack::type::raw_ref, uint64_t> res(
				form.raw_ref(), form.clocktime()
				);
		response.result(res, life);

	} else {
		LOG_DEBUG("key not found");
		response.null();
	}
}
RPC_CATCH(Set, response)


RPC_FUNC(Set, from, response, life, param)
try {
	LOG_DEBUG("Set '",std::string(param.key(),param.keylen()),"' => '",
			std::string(param.meta_val()+DBFormat::LEADING_METADATA_SIZE,
				param.meta_vallen()-DBFormat::LEADING_METADATA_SIZE),"'");

	check_coordinator_assign(m_whs, param.key(), param.keylen());

	ClockTime ct(m_clock.now_incr());

	if(param.meta_vallen() < DBFormat::LEADING_METADATA_SIZE) {
		throw msgpack::type_error();
	}
	DBFormat::set_meta(const_cast<char*>(param.meta_val()), ct.get(), 0);

	m_db.set(param.key(), param.keylen(),
			param.meta_val(), param.meta_vallen());


	// Replication
	unsigned short* copy_required = life->allocate<unsigned short>(0);

	RetryReplicateSet* retry = life->allocate<RetryReplicateSet>(
			protocol::type::ReplicateSet(
				param.key(), param.keylen(),
				param.meta_val(), param.meta_vallen(),
				m_clock.get_incr())
			);

	using namespace mp::placeholders;
	retry->set_callback( BIND_RESPONSE(ResReplicateSet,
			retry,
			copy_required,
			response, ct.get()) );

	uint64_t x = HashSpace::hash(param.key(), param.keylen());
	EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE(addr(),
			m_whs, x, n, {
				retry->call(n, life, 10);
				++*copy_required;
			})
#ifdef SET_ASYNC
	*copy_required = 0;
#endif

	LOG_DEBUG("set copy required: ", *copy_required);
	if(*copy_required == 0) {
		response.result( msgpack::type::tuple<uint64_t>(ct.get()) );
	}
}
RPC_CATCH(Set, response)


RPC_FUNC(Delete, from, response, life, param)
try {
	LOG_DEBUG("Delete '",std::string(param.key(),param.keylen()),"'");

	check_coordinator_assign(m_whs, param.key(), param.keylen());

	if(!m_db.erase(param.key(), param.keylen())) {
		// the key is not stored
		response.result(false);
		return;
	}
	// erase succeeded

	ClockTime ct(m_clock.now_incr());

	// Replication
	unsigned short* copy_required = life->allocate<unsigned short>(0);

	RetryReplicateDelete* retry = life->allocate<RetryReplicateDelete>(
			protocol::type::ReplicateDelete(
				param.key(), param.keylen(),
				ct.get(), m_clock.get_incr())
			);

	using namespace mp::placeholders;
	retry->set_callback( BIND_RESPONSE(ResReplicateDelete,
				retry,
				copy_required,
				response) );
	
	uint64_t x = HashSpace::hash(param.key(), param.keylen());
	EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE(addr(),
			m_whs, x, node, {
				retry->call(node, life);
				++*copy_required;
			})
#ifdef GET_ASYNC
	*copy_required = 0;
#endif

	if(*copy_required == 0) {
		response.result(true);
	}
}
RPC_CATCH(Set, response)


RPC_REPLY(ResReplicateSet, from, res, err, life,
		RetryReplicateSet* retry,
		unsigned short* copy_required,
		rpc::weak_responder response, uint64_t clocktime)
{
	LOG_DEBUG("ResReplicateSet ",res,",",err," remain:",*copy_required);
	// retry if failed
	if(!err.is_nil()) {
		if(from) {
			// FIXME delayed retry?
			if(retry->retry_incr(5)) {  // FIXME 5
				retry->call(from, life);
				LOG_DEBUG("ReplicateSet failed: ",err,", retry ",retry->num_retried());
				return;
			}
		}
		response.null();
		LOG_ERROR("ReplicateSet failed: ",err);
	} else {
		LOG_DEBUG("ReplicateSet succeeded");
	}

	--*copy_required;
	if(*copy_required == 0) {
		LOG_DEBUG("send response ",*copy_required);
		response.result( msgpack::type::tuple<uint64_t>(clocktime) );
	}
}

RPC_REPLY(ResReplicateDelete, from, res, err, life,
		RetryReplicateDelete* retry,
		unsigned short* copy_required,
		rpc::weak_responder response)
{
	// retry if failed
	if(!err.is_nil()) {
		if(from) {
			// FIXME delayed retry?
			if(retry->retry_incr(5)) {  // FIXME 5
				retry->call(from, life);
				LOG_DEBUG("ReplicateDelete failed: ",err,", retry ",retry->num_retried());
				return;
			}
		}
		response.null();
		LOG_ERROR("ReplicateDelete failed: ",err);
	} else {
		LOG_DEBUG("ReplicateDelete succeeded");
	}

	--*copy_required;
	if(*copy_required == 0) {
		response.result(true);
	}
}


CLUSTER_FUNC(ReplicateSet, from, response, life, param)
try {
	LOG_TRACE("ReplicateSet");
	m_clock.update(param.clock());

	check_replicator_assign(m_whs, param.key(), param.keylen());

	DBFormat form(param.meta_val(), param.meta_vallen());

	char meta[DBFormat::LEADING_METADATA_SIZE];
	int32_t ret = m_db.get_header(param.key(), param.keylen(), meta, sizeof(meta));

	if(ret < (int32_t)sizeof(meta) || clocktime_same_or_new(meta, form.clocktime())) {
		// key is not stored OR stored key is old
		m_db.set(param.key(), param.keylen(),
				param.meta_val(), param.meta_vallen());
		response.result(true);

	} else {
		// key is overwritten while replicating
		// do nothing
		response.result(false);
	}
}
RPC_CATCH(ReplicateSet, response)

CLUSTER_FUNC(ReplicateDelete, from, response, life, param)
try {
	LOG_TRACE("ReplicateDelete");
	m_clock.update(param.clock());

	// FIXME check write-hash-space assignment?
	//check_replicator_assign(m_whs, paramn.key(), param.keylen());

	char meta[DBFormat::LEADING_METADATA_SIZE];
	int32_t ret = m_db.get_header(param.key(), param.keylen(), meta, sizeof(meta));

	if(ret < (int32_t)sizeof(meta)) {
		// key is not stored
		// do nothing
		response.result(true);

	} else if(clocktime_same_or_new(meta, param.clocktime())) {
		// stored key is old
		m_db.erase(param.key(), param.keylen());
		response.result(true);

	} else {
		// key is already deleted while replicating
		// do nothing
		LOG_TRACE("obsolete replicate push");
		response.result(false);
	}
}
RPC_CATCH(ReplicateDelete, response)


}  // namespace kumo

