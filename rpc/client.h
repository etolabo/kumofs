#ifndef RPC_CLIENT_H__
#define RPC_CLIENT_H__

#include "rpc/session.h"
#include "log/mlogger.h"  // FIXME
#include <mp/pthread.h>
#include <map>

namespace rpc {


template <typename Transport = transport, typename Session = session>
class client : public session_manager, public transport_manager {
public:
	typedef mp::shared_ptr<Session> shared_session;
	typedef mp::weak_ptr<Session> weak_session;

	typedef rpc::msgobj          msgobj;
	typedef rpc::method_id       method_id;
	typedef rpc::msgid_t         msgid_t;
	typedef rpc::shared_zone     shared_zone;

	typedef mp::function<void (shared_session, msgobj, msgobj, shared_zone)> callback_t;

public:
	client(unsigned short connect_timeout_steps,
			unsigned int reconnect_timeout_msec = 5*1000);
	virtual ~client();

	virtual void transport_lost(shared_session& s) { }

	virtual void connect_timeout(const address& addr, shared_session& s) { }

	virtual void dispatch(
			shared_session& from, weak_responder response,
			method_id method, msgobj param, shared_zone& life) = 0;

	virtual void dispatch_request(
			basic_shared_session& s, weak_responder response,
			method_id method, msgobj param, shared_zone& life);

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
	bool connect_session(const address& addr, shared_session& s,
			mp::pthread_scoped_lock* lk = NULL);

private:
	typedef std::multimap<address, weak_session> sessions_t;
	sessions_t m_sessions;

	mp::pthread_mutex m_session_mutex;

	struct unbound_entry {
		shared_session session;
		unsigned short timeout_steps;
		address addr;
	};

	typedef std::map<address, unbound_entry> unbounds_t;
	unbounds_t m_unbounds;

private:
	struct connect_pack {
		connect_pack(client* srv, const address& addr);
		static void callback(void* data, int fd);
	private:
		client* m_srv;
		address m_addr;
	};

public:
	void connect_success(const address& addr, int fd);
	void connect_failed(const address& addr, int error);

private:
	unsigned short m_connect_timeout_steps;
	unsigned int m_reconnect_timeout_msec;

public:
	virtual void transport_lost_notify(basic_shared_session& s);

private:
	client();
	client(const client&);
};


}  // namespace rpc

#include  "rpc/client_tmpl.h"

#endif /* rpc/client.h */

