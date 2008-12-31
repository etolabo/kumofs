#ifndef LOGIC_RPC_H__
#define LOGIC_RPC_H__

#include "logic/global.h"
#include "logic/base.h"
#include "rpc/client.h"
#include "rpc/server.h"
#include <memory>

namespace kumo {

using rpc::msgobj;
using rpc::msgid_t;
using rpc::method_id;

using rpc::address;
using rpc::auto_zone;
using rpc::shared_zone;

using rpc::weak_responder;
using rpc::basic_shared_session;
using rpc::shared_peer;

using mp::pthread_scoped_lock;
using mp::pthread_scoped_rdlock;
using mp::pthread_scoped_wrlock;

template <typename Logic>
class RPCBase : public wavy_server {
public:
	typedef Logic ServerClass;

	template <typename Config>
	static ServerClass& initialize(Config& cfg)
	{
		s_instance.reset(new ServerClass(cfg));
		return instance();
	}

	template <typename Config>
	RPCBase(Config& cfg)
	{
		init_wavy(cfg);
	}

	~RPCBase() { }

protected:
	void start_timeout_step(unsigned long interval)
	{
		struct timespec ts = {interval / 1000, interval % 1000 * 1000};
		wavy::timer(&ts, mp::bind(&Logic::step_timeout,
					static_cast<Logic*>(this)));
		LOG_TRACE("start timeout stepping interval = ",interval," usec");
	}

	void start_keepalive(unsigned long interval)
	{
		struct timespec ts = {interval / 1000, interval % 1000 * 1000};
		wavy::timer(&ts, mp::bind(&Logic::keep_alive,
					static_cast<Logic*>(this)));
		LOG_TRACE("start keepalive interval = ",interval," usec");
	}

public:
	static ServerClass& instance()
		{ return *s_instance; }

	static std::auto_ptr<ServerClass> s_instance;
};

template <typename ServerClass>
std::auto_ptr<ServerClass> RPCBase<ServerClass>::s_instance;


template <rpc::method_id Method, typename Parameter>
class retry_pack {
public:
	retry_pack(Parameter arg) :
		m_limit(0), m_arg(arg) { }

	void set_callback(rpc::callback_t callback)
	{
		m_callbck = callback;
	}

public:
	bool retry_incr(unsigned short limit)
	{
		return ++m_limit <= limit;
	}

	template <typename Sessin>
	void call(Sessin s, rpc::shared_zone& life, unsigned short timeout_steps = 10)
	{
		s->call(Method, m_arg, life, m_callbck, timeout_steps);
	}

	unsigned short num_retried() const
	{
		return m_limit;
	}

	Parameter& param()
	{
		return m_arg;
	}

private:
	unsigned short m_limit;
	Parameter m_arg;
	rpc::callback_t m_callbck;

private:
	retry_pack();
	retry_pack(const retry_pack&);
};


#define SESSION_IS_ACTIVE(SESSION) \
	(SESSION && !SESSION->is_lost())


#define SHARED_ZONE(life, z) shared_zone(z.release())


#define RPC_RETRY(NAME) \
	retry_pack<protocol::NAME, protocol::type::NAME>


#define RPC_DISPATCH(NAME) \
	case protocol::NAME: \
		NAME(from, response, param.as<protocol::type::NAME>(), z); \
		return
//		mp::iothreads::submit(
//				&ServerClass::NAME, this,
//				from, response, life,
//				param.as<protocol::type::NAME>());

#define RPC_DECL(NAME) \
	void NAME(shared_session& from, weak_responder, \
			protocol::type::NAME, auto_zone)

#define RPC_IMPL(CLASS, NAME, from, response, z, param) \
	void CLASS::NAME(shared_session& from, weak_responder response, \
			protocol::type::NAME param, auto_zone z)


#define CLUSTER_DECL(NAME) \
	void NAME(shared_node& from, weak_responder, \
			protocol::type::NAME, auto_zone)

#define CLUSTER_IMPL(CLASS, NAME, from, response, z, param) \
	void CLASS::NAME(shared_node& from, weak_responder response, \
			protocol::type::NAME param, auto_zone z)


#define RPC_REPLY_DECL(NAME, from, res, err, z, ...) \
	void NAME(basic_shared_session from, msgobj res, msgobj err, \
			shared_zone z, ##__VA_ARGS__);

#define RPC_REPLY_IMPL(CLASS, NAME, from, res, err, z, ...) \
	void CLASS::NAME(basic_shared_session from, msgobj res, msgobj err, \
			shared_zone z, ##__VA_ARGS__)


#define RPC_CATCH(NAME, response) \
catch (msgpack::type_error& e) { \
	LOG_ERROR(#NAME " FAILED: type error"); \
	try { \
		response.error((uint8_t)protocol::SERVER_ERROR); \
	} catch (...) { } \
	throw; \
} catch (std::exception& e) { \
	LOG_WARN(#NAME " FAILED: ",e.what()); \
	try { \
		response.error((uint8_t)protocol::SERVER_ERROR); \
	} catch (...) { } \
	throw; \
} catch (...) { \
	LOG_ERROR(#NAME " FAILED: unknown error"); \
	try { \
		response.error((uint8_t)protocol::UNKNOWN_ERROR); \
	} catch (...) { } \
	throw; \
}


}  // namespace kumo

#include "protocol.h"

#endif /* logic/rpc.h */

