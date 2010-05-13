//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#include "gateway/framework.h"
#include <assert.h>

namespace kumo {
namespace gateway {


mod_store_t::mod_store_t() { }

mod_store_t::~mod_store_t() { }


template <resource::hash_space_type Hs>
framework::shared_session resource::server_for(uint64_t h, unsigned int offset)
{
#if NUM_REPLICATION != 2
#error fix following code
#endif
	assert(offset == 0 || offset == 1 || offset == 2);

	pthread_scoped_rdlock hslk(m_hs_rwlock);

	if((Hs == HS_WRITE ? m_whs : m_rhs).empty()) {
		share->incr_error_renew_count();
		throw std::runtime_error("No server");
	}
	HashSpace::iterator it = (Hs == HS_WRITE ? m_whs : m_rhs).find(h);

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
	return net->get_session(addr);
}


void resource::incr_error_renew_count()
{
	LOG_DEBUG("increment error count ",m_error_count);
	if(m_error_count >= m_cfg_renew_threshold) {
		m_error_count = 0;
		net->mod_network.renew_hash_space();
	} else {
		++m_error_count;
	}
}


template <typename Callback, typename ResponseType>
static void submit_callback_trampoline(
		Callback callback, void* user, ResponseType res, shared_zone life)
{
	auto_zone z(new msgpack::zone());
	z->allocate<shared_zone>(life);
	(*callback)(user, res, z);
}

#define SUBMIT_CATCH(NAME) \
catch (std::exception& e) { \
	LOG_WARN("req" #NAME " FAILED: ",e.what()); \
	gate::res##NAME res; \
	res.error = 1; \
	wavy::submit(submit_callback_trampoline<gate::callback##NAME, gate::res##NAME>, \
			req.callback, req.user, res, req.life); \
} catch (...) { \
	LOG_WARN("req" #NAME " FAILED: unknown error"); \
	gate::res##NAME res; \
	res.error = 1; \
	wavy::submit(submit_callback_trampoline<gate::callback##NAME, gate::res##NAME>, \
			req.callback, req.user, res, req.life); \
}


void mod_store_t::Get(gate::req_get& req)
try {
	shared_zone life(req.life);
	if(!life) { life.reset(new msgpack::zone()); }

	msgtype::DBKey key(req.key, req.keylen, req.hash);
	msgtype::DBValue cached_val_buf;

	if(net->mod_cache.get(key, &cached_val_buf, life.get())) {
		msgtype::DBValue* cached_val = life->allocate<msgtype::DBValue>(cached_val_buf);

		rpc::retry<server::mod_store_t::GetIfModified>* retry =
			life->allocate< rpc::retry<server::mod_store_t::GetIfModified> >(
					server::mod_store_t::GetIfModified(key, cached_val_buf.clocktime())
					);

		retry->set_callback(
				BIND_RESPONSE(mod_store_t, GetIfModified, retry,
					req.callback, req.user, cached_val) );

		retry->call(share->server_for<resource::HS_READ>(req.hash), life, 10);

	} else {
		rpc::retry<server::mod_store_t::Get>* retry =
			life->allocate< rpc::retry<server::mod_store_t::Get> >(
					server::mod_store_t::Get(key)
					);

		retry->set_callback(
				BIND_RESPONSE(mod_store_t, Get, retry,
					req.callback, req.user) );

		retry->call(share->server_for<resource::HS_READ>(req.hash), life, 10);
	}
}
SUBMIT_CATCH(_get);


void mod_store_t::Set(gate::req_set& req)
try {
	shared_zone life(req.life);
	if(!life) { life.reset(new msgpack::zone()); }

	server::set_op_t op;
	uint64_t clocktime = 0;
	switch(req.operation) {
	case gate::OP_SET:
		op = server::OP_SET;
		break;
	case gate::OP_SET_ASYNC:
		op = server::OP_SET_ASYNC;
		break;
	case gate::OP_CAS:
		op = server::OP_CAS;
		clocktime = req.clocktime;
		break;
	case gate::OP_APPEND:
		op = server::OP_APPEND;
		break;
	case gate::OP_PREPEND:
		op = server::OP_PREPEND;
		break;
	}

	uint16_t meta = 0;
	rpc::retry<server::mod_store_t::Set>* retry =
		life->allocate< rpc::retry<server::mod_store_t::Set> >(
				server::mod_store_t::Set(op,
					msgtype::DBKey(req.key, req.keylen, req.hash),
					msgtype::DBValue(req.val, req.vallen, meta, clocktime))
				);

	retry->set_callback(
			BIND_RESPONSE(mod_store_t, Set, retry, req.callback, req.user) );
	retry->call(share->server_for<resource::HS_WRITE>(req.hash), life, 10);
}
SUBMIT_CATCH(_set);


void mod_store_t::Delete(gate::req_delete& req)
try {
	shared_zone life(req.life);
	if(!life) { life.reset(new msgpack::zone()); }

	rpc::retry<server::mod_store_t::Delete>* retry =
		life->allocate< rpc::retry<server::mod_store_t::Delete> >(
				server::mod_store_t::Delete(
					(share->cfg_async_replicate_delete() || req.async) ?
					 static_cast<server::store_flags>(server::store_flags_async()) :
					 static_cast<server::store_flags>(server::store_flags_none()),
					msgtype::DBKey(req.key, req.keylen, req.hash))
				);

	retry->set_callback(
			BIND_RESPONSE(mod_store_t, Delete, retry, req.callback, req.user) );
	retry->call(share->server_for<resource::HS_WRITE>(req.hash), life, 10);
}
SUBMIT_CATCH(_delete);


#define GATEWAY_CATCH(NAME, response_type) \
catch (msgpack::type_error& e) { \
	LOG_ERROR(#NAME " FAILED: type error"); \
	response_type res; \
	res.error = 1; \
	try { (*callback)(user, res, z); } catch (...) { } \
} catch (std::exception& e) { \
	LOG_WARN(#NAME " FAILED: ",e.what()); \
	response_type res; \
	res.error = 1; \
	try { (*callback)(user, res, z); } catch (...) { } \
} catch (...) { \
	LOG_WARN(#NAME " FAILED: unknown error"); \
	response_type res; \
	res.error = 1; \
	try { (*callback)(user, res, z); } catch (...) { } \
}


namespace {
template <resource::hash_space_type Hs, typename Parameter>
struct retry_after_callback {
	retry_after_callback(
			rpc::retry<Parameter>* retry, shared_zone life,
			uint64_t for_hash, unsigned int offset = 0) :
		m_for_hash(for_hash), m_offset(offset),
		m_retry(retry), m_life(life) { }

	void operator() ()
	{
		// FIXME HS_WRITE?
		m_retry->call(
				share->server_for<Hs>(m_for_hash, m_offset),
				m_life, 10);
	}

private:
	uint64_t m_for_hash;
	unsigned int m_offset;
	rpc::retry<Parameter>* m_retry;
	shared_zone m_life;
};

template <resource::hash_space_type Hs, typename Parameter>
void retry_after(unsigned int steps,
		rpc::retry<Parameter>* retry, shared_zone life,
		uint64_t for_hash, unsigned int offset = 0)
{
	net->do_after(steps,
			retry_after_callback<Hs, Parameter>(retry, life, for_hash, offset));
}
}  // noname namespace


RPC_REPLY_IMPL(mod_store_t, Get, from, res, err, z,
		rpc::retry<server::mod_store_t::Get>* retry,
		gate::callback_get callback, void* user)
try {
	msgtype::DBKey key(retry->param().dbkey);
	LOG_TRACE("ResGet ",err);

	if(err.is_nil()) {
		gate::res_get ret;
		ret.error     = 0;
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		if(res.is_nil()) {
			ret.val       = NULL;
			ret.vallen    = 0;
			ret.clocktime = 0;
		} else {
			msgtype::DBValue st = res.as<msgtype::DBValue>();
			ret.val       = (char*)st.data();
			ret.vallen    = st.size();
			ret.clocktime = st.clocktime().get();
			net->mod_cache.update(key, st);
		}
		try { (*callback)(user, ret, z); } catch (...) { }

	} else if( retry->retry_incr((NUM_REPLICATION+1) * share->cfg_get_retry_num() - 1) ) {
		share->incr_error_renew_count();
		unsigned short offset = retry->num_retried() % (NUM_REPLICATION+1);
		SHARED_ZONE(life, z);
		if(offset == 0) {
			// FIXME configurable steps
			retry_after<resource::HS_READ>(1*framework::DO_AFTER_BY_SECONDS,
					retry, life, key.hash(), offset);
		} else {
			retry->call(share->server_for<resource::HS_READ>(key.hash(), offset), life, 10);
		}
		LOG_WARN("Get error: ",err,", fallback to offset +",offset," node");

	} else {
		if(err.via.u64 == (uint64_t)rpc::protocol::TRANSPORT_LOST_ERROR ||
				err.via.u64 == (uint64_t)rpc::protocol::SERVER_ERROR) {
			net->mod_network.renew_hash_space();   // FIXME
		}
		gate::res_get ret;
		ret.error     = 1;  // ERROR
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.val       = NULL;
		ret.vallen    = 0;
		ret.clocktime = 0;
		try { (*callback)(user, ret, z); } catch (...) { }
		TLOGPACK("eg",3,
				"key",msgtype::raw_ref(key.data(),key.size()),
				"err",err.via.u64);
		LOG_ERROR("Get error: ", err);
	}
}
GATEWAY_CATCH(ResGet, gate::res_get)


RPC_REPLY_IMPL(mod_store_t, GetIfModified, from, res, err, z,
		rpc::retry<server::mod_store_t::GetIfModified>* retry,
		gate::callback_get callback, void* user,
		msgtype::DBValue* cached_val)
try {
	// FIXME copied code
	msgtype::DBKey key(retry->param().dbkey);
	LOG_TRACE("ResGetIfModified ",err);

	if(err.is_nil()) {
		gate::res_get ret;
		ret.error     = 0;
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		if(res.is_nil()) {
			ret.val       = NULL;
			ret.vallen    = 0;
			ret.clocktime = 0;
		} else if(res.type == msgpack::type::BOOLEAN &&
				res.via.boolean == true) {
			// cached
			ret.val       = (char*)cached_val->data();
			ret.vallen    = cached_val->size();
			ret.clocktime = cached_val->clocktime().get();
		} else {
			msgtype::DBValue st = res.as<msgtype::DBValue>();
			ret.val       = (char*)st.data();
			ret.vallen    = st.size();
			ret.clocktime = st.clocktime().get();
			net->mod_cache.update(key, st);
		}
		try { (*callback)(user, ret, z); } catch (...) { }

	} else if( retry->retry_incr((NUM_REPLICATION+1) * share->cfg_get_retry_num() - 1) ) {
		share->incr_error_renew_count();
		unsigned short offset = retry->num_retried() % (NUM_REPLICATION+1);
		SHARED_ZONE(life, z);
		if(offset == 0) {
			// FIXME configurable steps
			retry_after<resource::HS_READ>(1*framework::DO_AFTER_BY_SECONDS,
					retry, life, key.hash(), offset);
		} else {
			retry->call(share->server_for<resource::HS_READ>(key.hash(), offset), life, 10);
		}
		LOG_WARN("Get error: ",err,", fallback to offset +",offset," node");

	} else {
		if(err.via.u64 == (uint64_t)rpc::protocol::TRANSPORT_LOST_ERROR ||
				err.via.u64 == (uint64_t)rpc::protocol::SERVER_ERROR) {
			net->mod_network.renew_hash_space();   // FIXME
		}
		gate::res_get ret;
		ret.error     = 1;  // ERROR
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.val       = NULL;
		ret.vallen    = 0;
		ret.clocktime = 0;
		try { (*callback)(user, ret, z); } catch (...) { }
		TLOGPACK("eg",3,
				"key",msgtype::raw_ref(key.data(),key.size()),
				"err",err.via.u64);
		LOG_ERROR("Get error: ", err);
	}
}
GATEWAY_CATCH(ResGet, gate::res_get)


RPC_REPLY_IMPL(mod_store_t, Set, from, res, err, z,
		rpc::retry<server::mod_store_t::Set>* retry,
		gate::callback_set callback, void* user)
try {
	msgtype::DBKey key(retry->param().dbkey);
	msgtype::DBValue val(retry->param().dbval);
	LOG_TRACE("ResSet ",err);

	if(!res.is_nil()) {
		gate::res_set ret;
		ret.error     = 0;
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.val       = val.data();
		ret.vallen    = val.size();
		if(res.type == msgpack::type::BOOLEAN && res.via.boolean == false) {
			ret.cas_success = false;
			ret.clocktime = 0;
		} else {
			ret.cas_success = true;
			ret.clocktime = res.as<ClockTime>().get();
		}
		//net->mod_cache.update(key, val);  // FIXME raw_data() is invalid
		try { (*callback)(user, ret, z); } catch (...) { }

	} else if( retry->retry_incr(share->cfg_set_retry_num()) ) {
		share->incr_error_renew_count();
		// FIXME configurable steps
		SHARED_ZONE(life, z);
		retry_after<resource::HS_WRITE>(1*framework::DO_AFTER_BY_SECONDS,
				retry, life, key.hash());
		LOG_WARN("Set error: ",err,", retry ",retry->num_retried());

	} else {
		if(err.via.u64 == (uint64_t)rpc::protocol::TRANSPORT_LOST_ERROR ||
				err.via.u64 == (uint64_t)rpc::protocol::SERVER_ERROR) {
			net->mod_network.renew_hash_space();   // FIXME
		}
		gate::res_set ret;
		ret.error     = 1;  // ERROR
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.val       = val.data();
		ret.vallen    = val.size();
		ret.clocktime = 0;
		try { (*callback)(user, ret, z); } catch (...) { }
		TLOGPACK("es",3,
				"key",msgtype::raw_ref(key.data(),key.size()),
				"val",msgtype::raw_ref(val.data(),val.size()),
				"err",err.via.u64);
		LOG_ERROR("Set error: ",err);
	}
}
GATEWAY_CATCH(ResSet, gate::res_set)


RPC_REPLY_IMPL(mod_store_t, Delete, from, res, err, z,
		rpc::retry<server::mod_store_t::Delete>* retry,
		gate::callback_delete callback, void* user)
try {
	msgtype::DBKey key(retry->param().dbkey);
	LOG_TRACE("ResDelete ",err);

	if(!res.is_nil()) {
		bool st = res.as<bool>();
		gate::res_delete ret;
		ret.error     = 0;
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.deleted   = st;
		try { (*callback)(user, ret, z); } catch (...) { }

	} else if( retry->retry_incr(share->cfg_delete_retry_num()) ) {
		share->incr_error_renew_count();
		// FIXME configurable steps
		SHARED_ZONE(life, z);
		retry_after<resource::HS_WRITE>(1*framework::DO_AFTER_BY_SECONDS,
				retry, life, key.hash());
		LOG_WARN("Delete error: ",err,", retry ",retry->num_retried());

	} else {
		if(err.via.u64 == (uint64_t)rpc::protocol::TRANSPORT_LOST_ERROR ||
				err.via.u64 == (uint64_t)rpc::protocol::SERVER_ERROR) {
			net->mod_network.renew_hash_space();   // FIXME
		}
		gate::res_delete ret;
		ret.error     = 1;  // ERROR
		ret.key       = key.data();
		ret.keylen    = key.size();
		ret.hash      = key.hash();
		ret.deleted   = false;
		try { (*callback)(user, ret, z); } catch (...) { }
		TLOGPACK("ed",3,
				"key",msgtype::raw_ref(key.data(),key.size()),
				"err",err.via.u64);
		LOG_ERROR("Delete error: ",err);
	}
}
GATEWAY_CATCH(ResDelete, gate::res_delete)


}  // namespace gateway
}  // namespace kumo

