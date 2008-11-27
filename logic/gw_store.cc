#include "logic/gw_impl.h"

namespace kumo {

inline Gateway::shared_session Gateway::server_for(const char* key, uint32_t keylen)
{
	return server_for(key, keylen, 0);
}

Gateway::shared_session Gateway::server_for(const char* key, uint32_t keylen, unsigned int offset)
{
#if NUM_REPLICATION != 2
#error fix following code
#endif
	assert(offset == 0 || offset == 1 || offset == 2);

	if(m_hs.empty()) {
		renew_hash_space();  // FIXME may burst
		throw std::runtime_error("No server");
	}
	uint64_t x = HashSpace::hash(key, keylen);
	HashSpace::iterator it( m_hs.find(x) );

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
	return get_server(it->addr());
}


#define GATEWAY_CATCH(NAME, response_type) \
catch (std::runtime_error& e) { \
	LOG_WARN(#NAME " FAILED: ",e.what()); \
	response_type res; \
	res.life = life; \
	res.error = 1; \
	(*callback)(user, res); \
	throw; \
} catch (std::bad_alloc& e) { \
	LOG_WARN(#NAME " FAILED: bad alloc"); \
	response_type res; \
	res.life = life; \
	res.error = 1; \
	(*callback)(user, res); \
	throw; \
} catch (...) { \
	LOG_WARN(#NAME " FAILED: unknown error"); \
	response_type res; \
	res.life = life; \
	res.error = 1; \
	(*callback)(user, res); \
	throw; \
}


void Gateway::Get(void (*callback)(void*, get_response&), void* user,
		shared_zone life,
		const char* key, uint32_t keylen)
try {
	RetryGet* retry = life->allocate<RetryGet>(
			protocol::type::Get(key, keylen) );

	retry->set_callback( BIND_RESPONSE(ResGet, retry, callback, user) );
	retry->call(server_for(key, keylen), life, 10);
}
GATEWAY_CATCH(Get, get_response)


void Gateway::Set(void (*callback)(void*, set_response&), void* user,
		shared_zone life,
		const char* key, uint32_t keylen,
		const char* val, uint32_t vallen)
try {
	RetrySet* retry = life->allocate<RetrySet>(
			protocol::type::Set(key, keylen, val, vallen) );

	retry->set_callback( BIND_RESPONSE(ResSet, retry, callback, user) );
	retry->call(server_for(key, keylen), life, 10);
}
GATEWAY_CATCH(Set, set_response)


void Gateway::Delete(void (*callback)(void*, delete_response&), void* user,
		shared_zone life,
		const char* key, uint32_t keylen)
try {
	RetryDelete* retry = life->allocate<RetryDelete>(
			protocol::type::Delete(key, keylen) );

	retry->set_callback( BIND_RESPONSE(ResDelete, retry, callback, user) );
	retry->call(server_for(key, keylen), life, 10);
}
GATEWAY_CATCH(Delete, delete_response)


RPC_REPLY(ResGet, from, res, err, life,
		RetryGet* retry,
		void (*callback)(void*, get_response&), void* user)
{
	if(err.is_nil()) {
		get_response ret;
		ret.error     = 0;
		ret.life      = life;
		ret.key       = retry->param().key();
		ret.keylen    = retry->param().keylen();
		if(res.is_nil()) {
			ret.val       = NULL;
			ret.vallen    = 0;
			ret.clocktime = 0;
			(*callback)(user, ret);
		} else {
			msgpack::type::tuple<msgpack::type::raw_ref, uint64_t> st(res);
			ret.val       = (char*)st.get<0>().ptr;
			ret.vallen    = st.get<0>().size;
			ret.clocktime = st.get<1>();
		}
		(*callback)(user, ret);

	} else if( retry->retry_incr((NUM_REPLICATION+1) * m_cfg_get_retry_num - 1) ) {
		unsigned short offset = retry->num_retried() % (NUM_REPLICATION+1);
		retry->call(
				server_for(retry->param().key(), retry->param().keylen(), offset),
				life, 10);
		LOG_INFO("Get error: ",err,", fallback to offset +",offset," node");

	} else {
		get_response ret;
		ret.error     = 1;  // ERROR
		ret.life      = life;
		ret.key       = retry->param().key();
		ret.keylen    = retry->param().keylen();
		ret.val       = NULL;
		ret.vallen    = 0;
		ret.clocktime = 0;
		(*callback)(user, ret);
		LOG_ERROR("Get error: ", err);
	}
}


RPC_REPLY(ResSet, from, res, err, life,
		RetrySet* retry,
		void (*callback)(void*, set_response&), void* user)
{
	if(!res.is_nil()) {
		msgpack::type::tuple<uint64_t> st(res);
		set_response ret;
		ret.error     = 0;
		ret.life      = life;
		ret.key       = retry->param().key();
		ret.keylen    = retry->param().keylen();
		ret.val       = retry->param().meta_val();
		ret.vallen    = retry->param().meta_vallen();
		ret.clocktime = st.get<0>();
		(*callback)(user, ret);

	} else if( retry->retry_incr(m_cfg_set_retry_num) ) {
		if(from->is_lost()) {
			// FIXME renew hash space?
			// FIXME delayed retry
			from = server_for(retry->param().key(), retry->param().keylen());
		}
		retry->call(from, life, 10);
		LOG_WARN("Set error: ",err,", retry ",retry->num_retried());

	} else {
		renew_hash_space();   // FIXME
		set_response ret;
		ret.error     = 1;  // ERROR
		ret.life      = life;
		ret.key       = retry->param().key();
		ret.keylen    = retry->param().keylen();
		ret.val       = retry->param().meta_val();
		ret.vallen    = retry->param().meta_vallen();
		ret.clocktime = 0;
		(*callback)(user, ret);
		LOG_ERROR("Set error: ",err);
	}
}


RPC_REPLY(ResDelete, from, res, err, life,
		RetryDelete* retry,
		void (*callback)(void*, delete_response&), void* user)
{
	if(!res.is_nil()) {
		bool st(res.convert());
		delete_response ret;
		ret.error     = 0;
		ret.life      = life;
		ret.key       = retry->param().key();
		ret.keylen    = retry->param().keylen();
		ret.deleted   = st;
		(*callback)(user, ret);

	} else if( retry->retry_incr(m_cfg_set_retry_num) ) {
		if(from->is_lost()) {
			// FIXME renew hash space?
			// FIXME delayed retry
			from = server_for(retry->param().key(), retry->param().keylen());
		}
		retry->call(from, life, 10);
		LOG_WARN("Delete error: ",err,", retry ",retry->num_retried());

	} else {
		renew_hash_space();   // FIXME
		delete_response ret;
		ret.error     = 1;  // ERROR
		ret.life      = life;
		ret.key       = retry->param().key();
		ret.keylen    = retry->param().keylen();
		ret.deleted   = false;
		(*callback)(user, ret);
		LOG_ERROR("Delete error: ",err);
	}
}



}  // namespace kumo

