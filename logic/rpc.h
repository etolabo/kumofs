#ifndef LOGIC_RPC_H__
#define LOGIC_RPC_H__

#include "logic/global.h"
#include "logic/base.h"
#include "rpc/client.h"
#include <memory>

namespace kumo {

using rpc::address;

namespace iothreads {
	using namespace mp::iothreads;
}


template <typename Logic>
class RPCBase : public iothreads_server {
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
		init_iothreads(cfg);
	}

	~RPCBase() { }

public:
	static ServerClass& instance()
		{ return *s_instance; }

	static std::auto_ptr<ServerClass> s_instance;
};

template <typename ServerClass>
std::auto_ptr<ServerClass> RPCBase<ServerClass>::s_instance;


// avoid compile error
	template <typename T>
inline void start_timeout_step(unsigned long interval, T* self)
{
	void (*f)(void*) = &mp::object_callback<void ()>
		::mem_fun<T, &T::step_timeout>;
	mp::iothreads::start_timer(interval,
		f, reinterpret_cast<void*>(self));
	LOG_TRACE("start timeout stepping interval = ",interval," usec");
}

template <typename T>
inline void start_keepalive(unsigned long interval, T* self)
{
	void (*f)(void*) = &mp::object_callback<void ()>
		::mem_fun<T, &T::keep_alive>;
	mp::iothreads::start_timer(interval,
		f, reinterpret_cast<void*>(self));
	LOG_TRACE("start keepalive interval = ",interval," usec");
}


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
	void call(Sessin s, rpc::shared_zone& life, unsigned short timeout_steps = 5)
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

#define RPC_RETRY(NAME) \
	retry_pack<protocol::NAME, protocol::type::NAME>


#define RPC_DISPATCH(NAME) \
	case protocol::NAME: \
		mp::iothreads::submit( \
				&ServerClass::NAME, this, \
				from, response, life, \
				param.as<protocol::type::NAME>()); \
		return


#define CLISRV_DISPATCH(NAME) \
	case protocol::NAME: \
		mp::iothreads::submit( \
				&ServerClass::NAME, m_srv, \
				from, response, life, \
				param.as<protocol::type::NAME>()); \
		return


#define RPC_DECL(NAME) \
	void NAME(shared_session from, rpc::weak_responder, shared_zone, protocol::type::NAME)

#define RPC_IMPL(CLASS, NAME, from, response, life, param) \
	void CLASS::NAME(shared_session from, rpc::weak_responder response, shared_zone life, protocol::type::NAME param)


#define CLUSTER_DECL(NAME) \
	void NAME(rpc::shared_node from, rpc::weak_responder, shared_zone, protocol::type::NAME)

#define CLUSTER_IMPL(CLASS, NAME, from, response, life, param) \
	void CLASS::NAME(rpc::shared_node from, rpc::weak_responder response, shared_zone life, protocol::type::NAME param)


#define CLISRV_DECL(NAME) \
	void NAME(rpc::shared_peer from, rpc::weak_responder, shared_zone, protocol::type::NAME)

#define CLISRV_IMPL(CLASS, NAME, from, response, life, param) \
	void CLASS::NAME(rpc::shared_peer from, rpc::weak_responder response, shared_zone life, protocol::type::NAME param)


#define RPC_REPLY_DECL(NAME, from, res, err, life, ...) \
	void NAME(rpc::basic_shared_session& from, msgobj res, msgobj err, shared_zone life, ##__VA_ARGS__);

#define RPC_REPLY_IMPL(CLASS, NAME, from, res, err, life, ...) \
	void CLASS::NAME(rpc::basic_shared_session& from, msgobj res, msgobj err, shared_zone life, ##__VA_ARGS__)


#define RPC_CATCH(NAME, response) \
catch (std::runtime_error& e) { \
	LOG_WARN(#NAME " FAILED: ",e.what()); \
	try { \
		response.error((uint8_t)protocol::SERVER_ERROR); \
	} catch (...) { } \
	throw; \
} catch (msgpack::type_error& e) { \
	LOG_WARN(#NAME " FAILED: type error"); \
	try { \
		response.error((uint8_t)protocol::PROTOCOL_ERROR); \
	} catch (...) { } \
	throw; \
} catch (std::bad_alloc& e) { \
	LOG_WARN(#NAME " FAILED: bad alloc"); \
	try { \
		response.error((uint8_t)protocol::SERVER_ERROR); \
	} catch (...) { } \
	throw; \
} catch (...) { \
	LOG_WARN(#NAME " FAILED: unknown error"); \
	try { \
		response.error((uint8_t)protocol::UNKNOWN_ERROR); \
	} catch (...) { } \
	throw; \
}


}  // namespace kumo

#include "protocol.h"

#endif /* logic/rpc.h */

