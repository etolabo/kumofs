#ifndef RPC_CLIENT_H__
#define RPC_CLIENT_H__

#include "rpc/rpc.h"
#include "rpc/protocol.h"
#include "log/mlogger.h"  // FIXME
#include <mp/pthread.h>
#include <map>

namespace rpc {


template <typename Transport = transport, typename Session = session>
class client : public session_manager, public transport_manager {
public:
	typedef mp::shared_ptr<Session> shared_session;
	typedef mp::weak_ptr<Session> weak_session;

	typedef mp::function<void (shared_session, msgobj, msgobj, shared_zone)> callback_t;

public:
	client(unsigned int connect_timeout_msec,
			unsigned short connect_retry_limit);
	virtual ~client();

	virtual void transport_lost(shared_session& s);

	virtual void connect_failed(shared_session s, address addr, int error)
	{
		transport_lost(s);
	}

	virtual void dispatch(
			shared_session& from, weak_responder response,
			method_id method, msgobj param, auto_zone z) = 0;

public:
	// step callback timeout count
	void step_timeout();

	// get/create RPC stub instance for the address.
	// if the session is not exist, connect to the session
	shared_session get_session(const address& addr);

	// get/create RPC stub instance for the address.
	// if the session is not exist, don't connect to the session
	// and returns unbound RPC stub instance.
	shared_session create_session(const address& addr);

	// add new connection and new managed Session and bind them.
	shared_session add(int fd, const address& addr);

protected:
	// connect session to the address and return true if
	// it is not bound.
	bool async_connect(const address& addr, shared_session& s);

private:
	template <bool CONNECT>
	shared_session get_session_impl(const address& addr);

private:
	mp::pthread_mutex m_sessions_mutex;
	typedef std::multimap<address, weak_session> sessions_t;
	sessions_t m_sessions;

	struct connect_pack {
		shared_session session;
		address addr;
	};

	void connect_callback(address addr, shared_session s, int fd, int err);
//	void connect_success(const address& addr, int fd);
//	void connect_failed(const address& addr, int error);

//	mp::pthread_mutex m_unbounds_mutex;
//	typedef std::map<address, unbound_entry> unbounds_t;
//	unbounds_t m_unbounds;

private:
	//struct connect_pack {
		//connect_pack(client* srv, const address& addr);
		//static void callback(void* data, int fd);
	//private:
		//client* m_srv;
		//address m_addr;
	//};

protected:
	unsigned int m_connect_timeout_msec;
	unsigned short m_connect_retry_limit;

public:
	virtual void dispatch_request(
			basic_shared_session& s, weak_responder response,
			method_id method, msgobj param, auto_zone z);

	virtual void transport_lost_notify(basic_shared_session& s);

private:
	client();
	client(const client&);
};


}  // namespace rpc

#include  "rpc/client_tmpl.h"

#endif /* rpc/client.h */

