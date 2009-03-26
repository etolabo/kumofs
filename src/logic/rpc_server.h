#ifndef LOGIC_RPC_SERVER_H__
#define LOGIC_RPC_SERVER_H__

#include "logic/wavy_server.h"
#include "rpc/rpc.h"
#include <mp/object_callback.h>

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

	// precision of the timer thread
	static const unsigned long TIMER_PRECISION_USEC = 500 * 1000;  // 0.5 sec.
	static const unsigned long DO_AFTER_BY_SECONDS = 1000*1000 / TIMER_PRECISION_USEC;

protected:
	// call Framework::step_timeout() every `interval_usec' microseconds.
	void start_timeout_step(unsigned long interval_usec)
	{
		m_timer_interval_steps = interval_usec / TIMER_PRECISION_USEC;
		m_timer_remain_steps = m_timer_interval_steps;
		struct timespec ts = {TIMER_PRECISION_USEC / 1000000, TIMER_PRECISION_USEC % 1000000 * 1000};
		wavy::timer(&ts, mp::bind(&Framework::timer_handler,
					static_cast<Framework*>(this)));
		LOG_TRACE("start timeout stepping interval = ",interval_usec," usec");
	}

protected:
	void timer_handler()
	{
		if(m_timer_remain_steps == 0) {
			try {
				static_cast<Framework*>(this)->step_timeout();
			} catch (...) { }
			m_timer_remain_steps = m_timer_interval_steps;
		} else {
			--m_timer_remain_steps;
		}

		static_cast<Framework*>(this)->step_do_after();
	}

	unsigned long m_timer_remain_steps;
	unsigned long m_timer_interval_steps;
};


struct unknown_method_error : msgpack::type_error { };


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


#define RPC_REPLY_DECL(NAME, from, res, err, z, ...) \
	void res_##NAME(basic_shared_session from, rpc::msgobj res, rpc::msgobj err, \
			auto_zone z, ##__VA_ARGS__);

#define RPC_REPLY_IMPL(SCOPE, NAME, from, res, err, z, ...) \
	void SCOPE::res_##NAME(basic_shared_session from, rpc::msgobj res, rpc::msgobj err, \
			auto_zone z, ##__VA_ARGS__)


#define BIND_RESPONSE(SCOPE, NAME, ...) \
	mp::bind(&SCOPE::res_##NAME, this, _1, _2, _3, _4, ##__VA_ARGS__)



#define DISPATCH_CATCH(method, response) \
catch (unknown_method_error& e) { \
	try { \
		response.error((uint8_t)rpc::protocol::PROTOCOL_ERROR); \
	} catch (...) { } \
	LOG_ERROR("method ",method.protocol(),".",method.version(), \
			" error : unknown method"); \
	throw; \
} catch (msgpack::type_error& e) { \
	try { \
		response.error((uint8_t)rpc::protocol::PROTOCOL_ERROR); \
	} catch (...) { } \
	LOG_ERROR("method ",method.protocol(),".",method.version(), \
			" error: type error"); \
	throw; \
} catch (std::exception& e) { \
	try { \
		response.error((uint8_t)rpc::protocol::SERVER_ERROR); \
	} catch (...) { } \
	LOG_WARN("method ",method.protocol(),".",method.version(), \
			" error: ",e.what()); \
	throw; \
} catch (...) { \
	try { \
		response.error((uint8_t)rpc::protocol::UNKNOWN_ERROR); \
	} catch (...) { } \
	LOG_ERROR("method ",method.protocol(),".",method.version(), \
			" error: unknown error"); \
	throw; \
}


}  // namespace kumo

#endif /* logic/rpc_server.h */

