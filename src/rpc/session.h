#ifndef RPC_SESSION_H__
#define RPC_SESSION_H__

#include "rpc/address.h"
#include "rpc/connection.h"
#include "rpc/vrefbuffer.h"
#include <mp/memory.h>
#include <mp/object_callback.h>
#include <algorithm>

namespace rpc {


class basic_transport;

struct session_manager {
	session_manager() { }

	virtual ~session_manager() { }

	virtual void dispatch_request(
			basic_shared_session& s, weak_responder response,
			method_id method, msgobj param, auto_zone z) = 0;

	virtual void transport_lost_notify(basic_shared_session& s) = 0;
};


class basic_session {
public:
	basic_session(session_manager* mgr = NULL);
	virtual ~basic_session();

	typedef std::auto_ptr<msgpack::zone> auto_zone;

public:
	// step callback timeout count.
	void step_timeout(basic_shared_session self);

	// return true if this session is connected.
	bool is_bound() const;

	// call remote procedure.
	// if this session is not bound, exception will be thrown.
	// Message is requred to inherit rpc::message.
	template <typename Message>
	void call(Message& params,
			shared_zone life, callback_t callback,
			unsigned short timeout_steps);

	// get session manager
	session_manager* get_manager();

	void send_data(const char* buf, size_t buflen,
			void (*finalize)(void*), void* data);

	void send_datav(vrefbuffer* buf,
			void (*finalize)(void*), void* data);

public:
	// called from client::async_connect and user.
	// the number of retried times is reset when bind_transport()
	// is called.
	unsigned short increment_connect_retried_count();

	// return number of connect retried times.
	unsigned short connect_retried_count();

	// called from user.
	// close this session.
	void shutdown();

public:
	// call all registered callback functions with specified arguments
	// and set is_lost == true
	void force_lost(msgobj res, msgobj err);

public:
	// return true if the destructor of this session is already running or
	// force_lost() is called.
	bool is_lost() const;

	// turn off the lost flag.
	// use this function carefully.
	void revive();

public:
	// called from transport
	void process_request(
			basic_shared_session& s,
			method_id method, msgobj param,
			msgid_t msgid, auto_zone z);

	// process callback.
	void process_response(
			basic_shared_session& self,
			msgobj result, msgobj error,
			msgid_t msgid, auto_zone z);

	virtual bool bind_transport(basic_transport* t);
	virtual bool unbind_transport(basic_transport* t, basic_shared_session& self);

protected:
	template <typename Message>
	msgid_t pack(vrefbuffer& buffer, Message& param);

private:
	class callback_entry {
	public:
		callback_entry();
		callback_entry(callback_t callback, shared_zone life,
				unsigned short timeout_steps);
	public:
		void callback(basic_shared_session& s, msgobj res, msgobj err, auto_zone& z);
		void callback(basic_shared_session& s, msgobj res, msgobj err);
		inline void callback_submit(basic_shared_session& s, msgobj res, msgobj err);
		inline bool step_timeout();  // Note: NOT thread-safe
	private:
		inline void callback_real(basic_shared_session& s,
				msgobj res, msgobj err, shared_zone z);
	private:
		unsigned short m_timeout_steps;
		callback_t m_callback;
		shared_zone m_life;
	};

protected:
	msgid_t m_msgid_rr;

	class callback_table {
	public:
		callback_table() { }
		~callback_table() { }
	public:
		void insert(msgid_t msgid, const callback_entry& entry);
		bool out(msgid_t msgid, callback_entry* result);
		template <typename F> void for_each_clear(F f);
		template <typename F> void erase_if(F f);
	public:
		static const size_t PARTITION_NUM = 4;  // FIXME
		typedef std::map<msgid_t, callback_entry> callbacks_t;
		mp::pthread_mutex m_callbacks_mutex[PARTITION_NUM];
		callbacks_t m_callbacks[PARTITION_NUM];
	private:
		callback_table(const callback_table&);
	};
	callback_table m_cbtable;

	mp::pthread_mutex m_binds_mutex;
	typedef std::vector<basic_transport*> binds_t;
	binds_t m_binds;

	bool m_lost;
	unsigned short m_connect_retried_count;

	session_manager* m_manager;

private:
	basic_session();
	basic_session(const basic_session&);
};


class session : public basic_session {
public:
	session(session_manager* mgr = NULL);
	virtual ~session();

public:
	// call remote procedure.
	// if this session is not connected, the request will
	// be kept till connected.
	// Message is requred to inherit rpc::message.
	template <typename Message>
	void call(Message& param,
			shared_zone life, callback_t callback,
			unsigned short timeout_steps);

	// clear all pending requests.
	void cancel_pendings();

public:
	virtual bool bind_transport(basic_transport* t);
	virtual bool unbind_transport(basic_transport* t, basic_shared_session& self);

private:
	mp::pthread_mutex m_pending_queue_mutex;
	typedef std::vector<vrefbuffer*> pending_queue_t;
	pending_queue_t m_pending_queue;
	void clear_pending_queue(pending_queue_t& queue);

private:
	session();
	session(const session&);
};


}  // namespace rpc

#endif /* rpc/session.h */

