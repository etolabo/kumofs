#ifndef LOGIC_RPC_SERVER_H__
#define LOGIC_RPC_SERVER_H__

#include "logic/wavy_server.h"
#include "rpc/rpc.h"
#include <mp/object_callback.h>
#include <mp/functional.h>

namespace kumo {


using namespace mp::placeholders;

//using rpc::msgobj;
//using rpc::msgid_t;
//using rpc::method_id;

using rpc::address;
using rpc::auto_zone;
using rpc::shared_zone;

using rpc::weak_responder;
using rpc::basic_shared_session;
//using rpc::shared_peer;

using mp::pthread_scoped_lock;
using mp::pthread_scoped_rdlock;
using mp::pthread_scoped_wrlock;


template <typename Framework>
class rpc_server : public wavy_server {
public:
	rpc_server(unsigned short rthreads, unsigned short wthreads)
	{
		init_wavy(rthreads, wthreads);
	}

	~rpc_server() { }

protected:
	void start_timeout_step(unsigned long interval)
	{
		struct timespec ts = {interval / 1000000, interval % 1000000 * 1000};
		wavy::timer(&ts, mp::bind(&Framework::step_timeout,
					static_cast<Framework*>(this)));
		LOG_TRACE("start timeout stepping interval = ",interval," usec");
	}
};


#define RESOURCE_CONST_ACCESSOR(TYPE, NAME) \
	inline const TYPE& NAME() const { return m_##NAME; }

#define RESOURCE_ACCESSOR(TYPE, NAME) \
	inline TYPE& NAME() { return m_##NAME; } \
	RESOURCE_CONST_ACCESSOR(TYPE, NAME)

#define SESSION_IS_ACTIVE(SESSION) \
	(SESSION && !SESSION->is_lost())


#define SHARED_ZONE(life, z) shared_zone life(z.release())


#define RPC_DISPATCH(SCOPE, NAME) \
	case SCOPE::NAME::method::id: \
		{ \
			rpc::request<SCOPE::NAME> req(from, param); \
			m_##SCOPE.rpc_##NAME(req, z, response); \
			break; \
		} \


#define RPC_IMPL(SCOPE, NAME, req, z, response) \
	void SCOPE::rpc_##NAME(rpc::request<NAME>& req, rpc::auto_zone z, \
			rpc::weak_responder response)


#define RPC_REPLY_DECL(NAME, from, res, err, life, ...) \
	void res_##NAME(basic_shared_session from, rpc::msgobj res, rpc::msgobj err, \
			shared_zone life, ##__VA_ARGS__);

#define RPC_REPLY_IMPL(SCOPE, NAME, from, res, err, life, ...) \
	void SCOPE::res_##NAME(basic_shared_session from, rpc::msgobj res, rpc::msgobj err, \
			shared_zone life, ##__VA_ARGS__)


#define BIND_RESPONSE(SCOPE, NAME, ...) \
	mp::bind(&SCOPE::res_##NAME, this, _1, _2, _3, _4, ##__VA_ARGS__)



#define RPC_CATCH(NAME, response) \
catch (msgpack::type_error& e) { \
	LOG_ERROR(#NAME " FAILED: type error"); \
	try { \
		response.error((uint8_t)rpc::protocol::SERVER_ERROR); \
	} catch (...) { } \
	throw; \
} catch (std::exception& e) { \
	LOG_WARN(#NAME " FAILED: ",e.what()); \
	try { \
		response.error((uint8_t)rpc::protocol::SERVER_ERROR); \
	} catch (...) { } \
	throw; \
} catch (...) { \
	LOG_ERROR(#NAME " FAILED: unknown error"); \
	try { \
		response.error((uint8_t)rpc::protocol::UNKNOWN_ERROR); \
	} catch (...) { } \
	throw; \
}
// FIXME more specific error


}  // namespace kumo

#endif /* logic/rpc_server.h */

