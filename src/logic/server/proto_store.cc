#include "server/framework.h"
#include "server/proto_control.h"

#define EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_ONE(EXCLUDE, HS, HASH, NODE, CODE) \
	EACH_ASSIGN(HS, HASH, _real_, \
			if(_real_.addr() != EXCLUDE && _real_.is_active()) { \
				shared_node NODE(net->get_node(_real_.addr())); \
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
					shared_node NODE(net->get_node(_real_.addr())); \
					CODE; \
				} \
			})

namespace kumo {
namespace server {


void proto_store::check_replicator_assign(HashSpace& hs, uint64_t h)
{
	if(hs.empty()) {
		throw std::runtime_error("server not ready");
	}
	EACH_ASSIGN(hs, h, r,
			if(r.is_active()) {  // don't write to fault node
				if(r.addr() == net->addr()) return;
			})
	throw std::runtime_error("obsolete hash space");
}

void proto_store::check_coordinator_assign(HashSpace& hs, uint64_t h)
{
	if(hs.empty()) {
		throw std::runtime_error("server not ready");
	}
	EACH_ASSIGN(hs, h, r,
			if(r.is_active()) {  // don't write to fault node
				if(r.addr() != net->addr())
					throw std::runtime_error("obsolete hash space");
				else
					return;
			})
}


RPC_IMPL(proto_store, Get_1, req, z, response)
try {
	msgtype::DBKey key(req.param().dbkey);
	LOG_DEBUG("Get '",
			/*std::string(key.data(),key.size()),*/"' with hash ",
			key.hash());

	{
		pthread_scoped_rdlock rhlk(share->rhs_mutex());
		check_replicator_assign(share->rhs(), key.hash());
	}

	uint32_t raw_vallen;
	const char* raw_val = share->db().get(
			key.raw_data(), key.raw_size(),
			&raw_vallen, z.get());

	if(raw_val) {
		LOG_DEBUG("key found");
		msgtype::raw_ref res(raw_val, raw_vallen);
		response.result(res, z);

	} else {
		LOG_DEBUG("key not found");
		response.null();
	}

	++share->stat_num_get();
}
RPC_CATCH(Get_1, response)


bool proto_store::SetByRhsWhs(weak_responder response, auto_zone& z,
		msgtype::DBKey& key, msgtype::DBValue& val,
		bool is_async)
{
	unsigned int rrep_num = 0;
	unsigned int wrep_num = 0;
	shared_node rrepto[NUM_REPLICATION];
	shared_node wrepto[NUM_REPLICATION];

	{
		pthread_scoped_rdlock whlk(share->whs_mutex());
		check_coordinator_assign(share->whs(), key.hash());

		pthread_scoped_rdlock rhlk(share->rhs_mutex());

		if(share->whs().clocktime() == share->rhs().clocktime()) {
			return false;
		}

		address wrep_addrs[NUM_REPLICATION+1];

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_ONE(net->addr(),
				share->whs(), key.hash(), n, {
					wrepto[wrep_num] = n;
					wrep_addrs[wrep_num] = n->addr();
					++wrep_num;
				})

		whlk.unlock();

		wrep_addrs[wrep_num] = net->addr();  // exclude self

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_N(wrep_addrs, wrep_num+1,
				share->rhs(), key.hash(), n, {
					rrepto[rrep_num++] = n;
				})
	}

	ClockTime ct(share->clock_incr().now());
	val.raw_set_clocktime(ct.get());

	volatile unsigned int* pcr =
		(volatile unsigned int*)z->malloc(sizeof(volatile unsigned int));
	if(is_async) { *pcr = 0; }
	else { *pcr = wrep_num + rrep_num; }

	using namespace mp::placeholders;

	// rhs Replication
	rpc::retry<ReplicateSet_1>* rretry =
		z->allocate< rpc::retry<ReplicateSet_1> >(
				ReplicateSet_1(
					ct.clock().get(), replicate_flags_by_rhs(),  // flags = by rhs
					msgtype::DBKey(key.raw_data(), key.raw_size()),
					msgtype::DBValue(val.raw_data(), val.raw_size()))
				);
	rretry->set_callback( BIND_RESPONSE(proto_store, ReplicateSet_1,
			rretry,
			pcr,
			response, ct.get()) );

	// whs Replication
	rpc::retry<ReplicateSet_1>* wretry =
		z->allocate< rpc::retry<ReplicateSet_1> >(
				ReplicateSet_1(
					ct.clock().get(), replicate_flags_none(),  // flags = none
					msgtype::DBKey(key.raw_data(), key.raw_size()),
					msgtype::DBValue(val.raw_data(), val.raw_size()))
				);
	wretry->set_callback( BIND_RESPONSE(proto_store, ReplicateSet_1,
			wretry,
			pcr,
			response, ct) );

	SHARED_ZONE(life, z);

	for(unsigned int i=0; i < rrep_num; ++i) {
		rretry->call(rrepto[i], life, 10);
	}

	for(unsigned int i=0; i < wrep_num; ++i) {
		wretry->call(wrepto[i], life, 10);
	}

	share->db().set(
			key.raw_data(), key.raw_size(),
			val.raw_data(), val.raw_size());

	LOG_DEBUG("set copy required: ", wrep_num+rrep_num);
	if((wrep_num == 0 && rrep_num == 0) || is_async) {
		response.result( msgtype::tuple<ClockTime>(ct.get()) );
	}

	return true;
}

void proto_store::SetByWhs(weak_responder response, auto_zone& z,
		msgtype::DBKey& key, msgtype::DBValue& val,
		bool is_async)
{
	unsigned int wrep_num = 0;
	shared_node wrepto[NUM_REPLICATION];

	{
		pthread_scoped_rdlock whlk(share->whs_mutex());
		check_coordinator_assign(share->whs(), key.hash());

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_ONE(net->addr(),
				share->whs(), key.hash(), n, {
					wrepto[wrep_num++] = n;
				})
	}

	ClockTime ct(share->clock_incr().now());
	val.raw_set_clocktime(ct.get());

	volatile unsigned int* pcr =
		(volatile unsigned int*)z->malloc(sizeof(volatile unsigned int));
	if(is_async) { *pcr = 0; }
	else { *pcr = wrep_num; }

	using namespace mp::placeholders;

	// whs Replication
	rpc::retry<ReplicateSet_1>* retry =
		z->allocate< rpc::retry<ReplicateSet_1> >(
				ReplicateSet_1(
					ct.clock().get(), replicate_flags_none(),  // flags = none
					msgtype::DBKey(key.raw_data(), key.raw_size()),
					msgtype::DBValue(val.raw_data(), val.raw_size()))
				);
	retry->set_callback( BIND_RESPONSE(proto_store, ReplicateSet_1,
			retry,
			pcr,
			response, ct.get()) );
	
	SHARED_ZONE(life, z);
	for(unsigned int i=0; i < wrep_num; ++i) {
		retry->call(wrepto[i], life, 10);
	}

	share->db().set(
			key.raw_data(), key.raw_size(),
			val.raw_data(), val.raw_size());

	LOG_DEBUG("set copy required: ", wrep_num);
	if(wrep_num == 0 || is_async) {
		response.result( msgtype::tuple<ClockTime>(ct.get()) );
	}
}

RPC_IMPL(proto_store, Set_1, req, z, response)
try {
	msgtype::DBKey key(req.param().dbkey);
	msgtype::DBValue val(req.param().dbval);
	LOG_DEBUG("Set '",
			/*std::string(key.data(),key.size()),*/"' => '",
			/*std::string(val.data(),val.size()),*/"' with hash ",
			key.hash(),", with meta ",val.meta());

	if(share->whs().clocktime() != share->rhs().clocktime()) {
		if( !SetByRhsWhs(response, z, key, val, req.param().flags.is_async()) ) {
			SetByWhs(response, z, key, val, req.param().flags.is_async());
		}
	} else {
		SetByWhs(response, z, key, val, req.param().flags.is_async());
	}

	++share->stat_num_set();
}
RPC_CATCH(Set_1, response)



bool proto_store::DeleteByRhsWhs(weak_responder response, auto_zone& z,
		msgtype::DBKey& key,
		bool is_async)
{
	unsigned int rrep_num = 0;
	unsigned int wrep_num = 0;
	shared_node rrepto[NUM_REPLICATION];
	shared_node wrepto[NUM_REPLICATION];

	{
		pthread_scoped_rdlock whlk(share->whs_mutex());
		check_coordinator_assign(share->whs(), key.hash());

		pthread_scoped_rdlock rhlk(share->rhs_mutex());

		if(share->whs().clocktime() == share->rhs().clocktime()) {
			return false;
		}

		address wrep_addrs[NUM_REPLICATION+1];

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_ONE(net->addr(),
				share->whs(), key.hash(), n, {
					wrepto[wrep_num] = n;
					wrep_addrs[wrep_num] = n->addr();
					++wrep_num;
				})

		whlk.unlock();

		wrep_addrs[wrep_num] = net->addr();  // exclude self

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_N(wrep_addrs, wrep_num+1,
				share->rhs(), key.hash(), n, {
					rrepto[rrep_num++] = n;
				})
	}

	ClockTime ct(share->clock_incr().now());

	bool deleted = share->db().remove(key.raw_data(), key.raw_size(), ct);
	if(!deleted) {
		//response.result(false);
		// the key is not stored
		//return true;
		wrep_num = 0;
	}

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
	rpc::retry<ReplicateDelete_1>* rretry =
		z->allocate< rpc::retry<ReplicateDelete_1> >(
				ReplicateDelete_1(
					ct.get(),
					ct.clock().get(),
					replicate_flags_by_rhs(),  // flag = by rhs
					msgtype::DBKey(key.raw_data(), key.raw_size()))
				);
	rretry->set_callback( BIND_RESPONSE(proto_store, ReplicateDelete_1,
				rretry,
				pcr,
				response, deleted) );

	// whs Replication
	rpc::retry<ReplicateDelete_1>* wretry =
		z->allocate< rpc::retry<ReplicateDelete_1> >(
				ReplicateDelete_1(
					ct.get(),
					ct.clock().get(),
					replicate_flags_none(),  // flag = none
					msgtype::DBKey(key.raw_data(), key.raw_size()))
				);
	wretry->set_callback( BIND_RESPONSE(proto_store, ReplicateDelete_1,
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

void proto_store::DeleteByWhs(weak_responder response, auto_zone& z,
		msgtype::DBKey& key,
		bool is_async)
{
	unsigned int wrep_num = 0;
	shared_node wrepto[NUM_REPLICATION];

	{
		pthread_scoped_rdlock whlk(share->whs_mutex());
		check_coordinator_assign(share->whs(), key.hash());

		EACH_ASSIGNED_ACTIVE_NODE_EXCLUDE_ONE(net->addr(),
				share->whs(), key.hash(), n, {
					wrepto[wrep_num++] = n;
				})
	}

	ClockTime ct(share->clock_incr().now());

	bool deleted = share->db().remove(key.raw_data(), key.raw_size(), ct);
	if(!deleted) {
		response.result(false);
		// the key is not stored
		return;
	}

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
	rpc::retry<ReplicateDelete_1>* retry =
		z->allocate< rpc::retry<ReplicateDelete_1> >(
				ReplicateDelete_1(
					ct.get(), ct.clock().get(),
					replicate_flags_none(),
					msgtype::DBKey(key.raw_data(), key.raw_size()))
				);
	retry->set_callback( BIND_RESPONSE(proto_store, ReplicateDelete_1,
			retry,
			pcr,
			response, deleted) );
	
	SHARED_ZONE(life, z);
	for(unsigned int i=0; i < wrep_num; ++i) {
		retry->call(wrepto[i], life, 10);
	}
}

RPC_IMPL(proto_store, Delete_1, req, z, response)
try {
	msgtype::DBKey key(req.param().dbkey);
	LOG_DEBUG("Delete '",
			std::string(key.data(),key.size()),"' with hash",
			key.hash());

	if(share->whs().clocktime() != share->rhs().clocktime()) {
		if( !DeleteByRhsWhs(response, z, key, req.param().flags.is_async()) ) {
			DeleteByWhs(response, z, key, req.param().flags.is_async());
		}
	} else {
		DeleteByWhs(response, z, key, req.param().flags.is_async());
	}

	++share->stat_num_delete();
}
RPC_CATCH(Delete_1, response)



RPC_REPLY_IMPL(proto_store, ReplicateSet_1, from, res, err, life,
		rpc::retry<ReplicateSet_1>* retry,
		volatile unsigned int* copy_required,
		rpc::weak_responder response, ClockTime clocktime)
{
	LOG_DEBUG("ResReplicateSet ",res,",",err," remain:",*copy_required);
	// retry if failed
	if(!err.is_nil()) {
		if(SESSION_IS_ACTIVE(from)) {
			// FIXME delayed retry?
			if(retry->retry_incr(share->cfg_replicate_set_retry_num())) {
				retry->call(from, life);
				LOG_WARN("ReplicateSet error: ",err,", retry ",retry->num_retried());
				return;
			}
		}
		if(!retry->param().flags.is_rhs()) {  // FIXME ?
			response.null();
			LOGPACK("ers",2,
					"key",msgtype::raw_ref(
						retry->param().dbkey.data(),
						retry->param().dbkey.size()),
					"val",msgtype::raw_ref(
						retry->param().dbval.data(),
						retry->param().dbval.size()));
			LOG_ERROR("ReplicateSet failed: ",err);
			return;
		}
	}

	LOG_DEBUG("ReplicateSet succeeded");

	if(__sync_sub_and_fetch(copy_required, 1) == 0) {
		response.result( msgtype::tuple<ClockTime>(clocktime) );
	}
}

RPC_REPLY_IMPL(proto_store, ReplicateDelete_1, from, res, err, life,
		rpc::retry<ReplicateDelete_1>* retry,
		volatile unsigned int* copy_required,
		rpc::weak_responder response, bool deleted)
{
	// retry if failed
	if(!err.is_nil()) {
		if(SESSION_IS_ACTIVE(from)) {
			// FIXME delayed retry?
			if(retry->retry_incr(share->cfg_replicate_delete_retry_num())) {
				retry->call(from, life);
				LOG_WARN("ReplicateDelete error: ",err,", retry ",retry->num_retried());
				return;
			}
		}
		if(!retry->param().flags.is_rhs()) {  // FIXME ?
			response.null();
			LOGPACK("erd",2,
					"key",msgtype::raw_ref(
						retry->param().dbkey.data(),
						retry->param().dbkey.size()));
			LOG_ERROR("ReplicateDelete failed: ",err);
			return;
		}
	}

	LOG_DEBUG("ReplicateDelete succeeded");

	if(__sync_sub_and_fetch(copy_required, 1) == 0) {
		if(!deleted && retry->param().flags.is_rhs() &&
				res.type == msgtype::BOOLEAN && res.via.boolean == true) {
			deleted = true;
		}
		response.result(deleted);
	}
}


RPC_IMPL(proto_store, ReplicateSet_1, req, z, response)
try {
	msgtype::DBKey key = req.param().dbkey;
	msgtype::DBValue val = req.param().dbval;
	LOG_TRACE("ReplicateSet");

	if(req.param().flags.is_rhs()) {
		pthread_scoped_rdlock rhlk(share->rhs_mutex());
		check_replicator_assign(share->rhs(), key.hash());
	} else {
		pthread_scoped_rdlock whlk(share->whs_mutex());
		check_replicator_assign(share->whs(), key.hash());
	}

	share->update_clock(req.param().clock);

	bool updated = share->db().update(
			key.raw_data(), key.raw_size(),
			val.raw_data(), val.raw_size());

	response.result(updated);
}
RPC_CATCH(ReplicateSet_1, response)


RPC_IMPL(proto_store, ReplicateDelete_1, req, z, response)
try {
	msgtype::DBKey key = req.param().dbkey;
	LOG_TRACE("ReplicateDelete");

	if(req.param().flags.is_rhs()) {
		pthread_scoped_rdlock rhlk(share->rhs_mutex());
		check_replicator_assign(share->rhs(), key.hash());
	} else {
		pthread_scoped_rdlock whlk(share->whs_mutex());
		check_replicator_assign(share->whs(), key.hash());
	}

	share->update_clock(req.param().clock);

	bool deleted = share->db().remove(key.raw_data(), key.raw_size(),
			req.param().clocktime);

	response.result(deleted);
}
RPC_CATCH(ReplicateDelete_1, response)



}  // namespace server
}  // namespace kumo

