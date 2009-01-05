#include "logic/gw_impl.h"
#include <assert.h>

namespace kumo {

template <Gateway::hash_space_type Hs>
Gateway::shared_session Gateway::server_for(uint64_t h, unsigned int offset)
{
#if NUM_REPLICATION != 2
#error fix following code
#endif
	assert(offset == 0 || offset == 1 || offset == 2);

	pthread_scoped_rdlock hslk(m_hs_rwlock);

	if((Hs == HS_WRITE ? m_whs : m_rhs).empty()) {
		renew_hash_space();  // FIXME may burst
		throw std::runtime_error("No server");
	}
	HashSpace::iterator it =
		(Hs == HS_WRITE ? m_whs : m_rhs).find(h);

	{
		if(offset == 0) {
			if(it->is_active()) { goto node_found; }
		} else { --offset; }
	
		HashSpace::iterator origin(it);
		++it;
		for(; it != origin; ++it) {
			if(*it == *origin) { continue; }
	
			if(offset == 0) {
				if(it->is_active()) { goto node_found; }
			} else { --offset; }
	
			HashSpace::node rep1 = *it;
			++it;
			for(; it != origin; ++it) {
				if(*it == *origin || *it == rep1) { continue; }
				HashSpace::node _rep2_ = *it;
	
				if(offset == 0) {
					if(it->is_active()) { goto node_found; }
				} else { --offset; }
	
				break;
			}
			break;
		}
	}

node_found:
	address addr = it->addr();
	hslk.unlock();
	return get_server(addr);
}


// FIXME submit callback?
#define GATEWAY_CATCH(NAME, response_type) \
catch (std::runtime_error& e) { \
	LOG_WARN(#NAME " FAILED: ",e.what()); \
	response_type res; \
	res.life = life; \
	res.error = 1; \
	wavy::submit(*callback, user, res); \
} catch (std::bad_alloc& e) { \
	LOG_WARN(#NAME " FAILED: bad alloc"); \
	response_type res; \
	res.life = life; \
	res.error = 1; \
	wavy::submit(*callback, user, res); \
} catch (...) { \
	LOG_WARN(#NAME " FAILED: unknown error"); \
	response_type res; \
	res.life = life; \
	res.error = 1; \
	wavy::submit(*callback, user, res); \
}


void Gateway::Get(void (*callback)(void*, get_response&), void* user,
		shared_zone life,
		const char* key, uint32_t keylen, uint64_t hash)
try {
	if(!life) { life.reset(new msgpack::zone()); }
	RetryGet* retry = life->allocate<RetryGet>(
			protocol::type::Get(
				protocol::type::DBKey(key, keylen, hash)
				));

	retry->set_callback( BIND_RESPONSE(ResGet, retry, callback, user) );
	retry->call(server_for<HS_READ>(hash), life, 10);
}
GATEWAY_CATCH(Get, get_response)


void Gateway::Set(void (*callback)(void*, set_response&), void* user,
		shared_zone life,
		const char* key, uint32_t keylen, uint64_t hash,
		const char* val, uint32_t vallen)
try {
	uint64_t meta = 0;
	if(!life) { life.reset(new msgpack::zone()); }
	RetrySet* retry = life->allocate<RetrySet>(
			protocol::type::Set(
				protocol::type::DBKey(key, keylen, hash),
				protocol::type::DBValue(val, vallen, meta)
				));

	retry->set_callback( BIND_RESPONSE(ResSet, retry, callback, user) );
	retry->call(server_for<HS_WRITE>(hash), life, 10);
}
GATEWAY_CATCH(Set, set_response)


void Gateway::Delete(void (*callback)(void*, delete_response&), void* user,
		shared_zone life,
		const char* key, uint32_t keylen, uint64_t hash)
try {
	if(!life) { life.reset(new msgpack::zone()); }
	RetryDelete* retry = life->allocate<RetryDelete>(
			protocol::type::Delete(
				protocol::type::DBKey(key, keylen, hash)
				));

	retry->set_callback( BIND_RESPONSE(ResDelete, retry, callback, user) );
	retry->call(server_for<HS_WRITE>(hash), life, 10);
}
GATEWAY_CATCH(Delete, delete_response)


RPC_REPLY(ResGet, from, res, err, life,
		RetryGet* retry,
		void (*callback)(void*, get_response&), void* user)
try {
	protocol::type::DBKey key(retry->param().dbkey());
	LOG_TRACE("ResGet ",err);

	if(err.is_nil()) {
		get_response ret;
		ret.error     = 0;
		ret.life      = life;
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		if(res.is_nil()) {
			ret.val       = NULL;
			ret.vallen    = 0;
			ret.clocktime = 0;
		} else {
			protocol::type::DBValue st(res.convert());
			ret.val       = (char*)st.data();
			ret.vallen    = st.size();
			ret.clocktime = st.clocktime();
		}
		try { (*callback)(user, ret); } catch (...) { }

	} else if( retry->retry_incr((NUM_REPLICATION+1) * m_cfg_get_retry_num - 1) ) {
		incr_error_count();
		unsigned short offset = retry->num_retried() % (NUM_REPLICATION+1);
		retry->call(server_for<HS_READ>(key.hash(), offset), life, 10);
		LOG_INFO("Get error: ",err,", fallback to offset +",offset," node");

	} else {
		if(err.via.u64 == (uint64_t)rpc::protocol::TRANSPORT_LOST_ERROR ||
				err.via.u64 == (uint64_t)rpc::protocol::SERVER_ERROR) {
			renew_hash_space();   // FIXME
		}
		get_response ret;
		ret.error     = 1;  // ERROR
		ret.life      = life;
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.val       = NULL;
		ret.vallen    = 0;
		ret.clocktime = 0;
		try { (*callback)(user, ret); } catch (...) { }
		LOG_ERROR("Get error: ", err);
	}
}
GATEWAY_CATCH(ResGet, get_response)


RPC_REPLY(ResSet, from, res, err, life,
		RetrySet* retry,
		void (*callback)(void*, set_response&), void* user)
try {
	protocol::type::DBKey key(retry->param().dbkey());
	protocol::type::DBValue val(retry->param().dbval());
	LOG_TRACE("ResSet ",err);

	if(!res.is_nil()) {
		msgpack::type::tuple<uint64_t> st(res);
		set_response ret;
		ret.error     = 0;
		ret.life      = life;
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.val       = val.data();
		ret.vallen    = val.size();
		ret.clocktime = st.get<0>();
		try { (*callback)(user, ret); } catch (...) { }

	} else if( retry->retry_incr(m_cfg_set_retry_num) ) {
		incr_error_count();
		if(!SESSION_IS_ACTIVE(from)) {
			// FIXME renew hash space?
			// FIXME delayed retry
			from = server_for<HS_WRITE>(key.hash());
		}
		retry->call(from, life, 10);
		LOG_WARN("Set error: ",err,", retry ",retry->num_retried());

	} else {
		if(err.via.u64 == (uint64_t)rpc::protocol::TRANSPORT_LOST_ERROR ||
				err.via.u64 == (uint64_t)rpc::protocol::SERVER_ERROR) {
			renew_hash_space();   // FIXME
		}
		set_response ret;
		ret.error     = 1;  // ERROR
		ret.life      = life;
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.val       = val.data();
		ret.vallen    = val.size();
		ret.clocktime = 0;
		try { (*callback)(user, ret); } catch (...) { }
		LOG_ERROR("Set error: ",err);
	}
}
GATEWAY_CATCH(ResSet, set_response)


RPC_REPLY(ResDelete, from, res, err, life,
		RetryDelete* retry,
		void (*callback)(void*, delete_response&), void* user)
try {
	protocol::type::DBKey key(retry->param().dbkey());
	LOG_TRACE("ResDelete ",err);

	if(!res.is_nil()) {
		bool st(res.convert());
		delete_response ret;
		ret.error     = 0;
		ret.life      = life;
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.deleted   = st;
		try { (*callback)(user, ret); } catch (...) { }

	} else if( retry->retry_incr(m_cfg_delete_retry_num) ) {
		incr_error_count();
		if(!SESSION_IS_ACTIVE(from)) {
			// FIXME renew hash space?
			// FIXME delayed retry
			from = server_for<HS_WRITE>(key.hash());
		}
		retry->call(from, life, 10);
		LOG_WARN("Delete error: ",err,", retry ",retry->num_retried());

	} else {
		if(err.via.u64 == (uint64_t)rpc::protocol::TRANSPORT_LOST_ERROR ||
				err.via.u64 == (uint64_t)rpc::protocol::SERVER_ERROR) {
			renew_hash_space();   // FIXME
		}
		delete_response ret;
		ret.error     = 1;  // ERROR
		ret.life      = life;
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.deleted   = false;
		try { (*callback)(user, ret); } catch (...) { }
		LOG_ERROR("Delete error: ",err);
	}
}
GATEWAY_CATCH(ResDelete, delete_response)


}  // namespace kumo

