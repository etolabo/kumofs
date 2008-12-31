#ifndef RPC_SESSION_IMPL_H__
#define RPC_SESSION_IMPL_H__

namespace rpc {


inline basic_session::basic_session(session_manager* mgr) :
	m_msgid_rr(0),  // FIXME randomize?
	m_lost(false),
	m_connect_retried_count(0),
	m_manager(mgr) { }


inline bool basic_session::is_bound() const
	{ return !m_binds.empty(); }

inline bool basic_session::is_lost() const
	{ return m_lost; }

inline void basic_session::revive()
	{ m_lost = false; }


inline void basic_session::increment_connect_retried_count()
	{ ++m_connect_retried_count; } // FIXME atomic?

inline unsigned short basic_session::connect_retried_count()
	{ return m_connect_retried_count; }


inline session::session(session_manager* mgr) :
	basic_session(mgr)
{ }

inline bool basic_session::empty() const
{
	return m_callbacks.empty();
}

inline bool session::empty() const
{
	return basic_session::empty() && m_pending_queue.empty();
}

inline session_manager* basic_session::get_manager()
{
	return m_manager;
}


inline void basic_session::process_request(
		basic_shared_session& s,
		method_id method, msgobj param,
		msgid_t msgid, auto_zone z)
{
	weak_responder response(s, msgid);
	get_manager()->dispatch_request(s, response, method, param, z);
}


template <typename Parameter>
msgid_t basic_session::pack(vrefbuffer& buffer, method_id method, Parameter& params)
{
	/*
	if(m_callbacks.size() >= std::numeric_limits<msgid_t>::max()) {
		throw std::runtime_error("too many requests are queued");
	}
	while(true) {
		++m_msgid_rr;
		if(m_callbacks.find(m_msgid_rr) == m_callbacks.end()) {
			break;
		}
	}

	rpc_request<Parameter> msgreq(method, params, m_msgid_rr);
	msgpack::pack(buffer, msgreq);

	return m_msgid_rr;
	*/
	// FIXME
	msgid_t msgid = __sync_add_and_fetch(&m_msgid_rr, 1);
	rpc_request<Parameter> msgreq(method, params, msgid);
	msgpack::pack(buffer, msgreq);
	return msgid;
}


template <typename Parameter>
void basic_session::call(
		method_id method, Parameter& params,
		shared_zone life, callback_t callback,
		unsigned short timeout_steps)
{
	if(is_lost()) { throw std::runtime_error("lost session"); }
	if(!life) { life.reset(new msgpack::zone()); }

	vrefbuffer* buf = life->allocate<vrefbuffer>();
	msgid_t msgid = pack(*buf, method, params);

	mp::pthread_scoped_lock lk(m_callbacks_mutex);
	m_callbacks[msgid] =  // FIXME m_callbacks.insert
		callback_entry(callback, life, timeout_steps);

	mp::pthread_scoped_lock blk(m_binds_mutex);
	if(m_binds.empty()) {
		m_callbacks.erase(msgid);
		throw std::runtime_error("session not bound");

	} else {
		// ad-hoc load balancing
		m_binds[m_msgid_rr % m_binds.size()]->send_datav(
				buf, NULL, NULL);
	}
}


template <typename Parameter>
void session::call(
		method_id method, Parameter& params,
		shared_zone life, callback_t callback,
		unsigned short timeout_steps)
{
	if(is_lost()) { throw std::runtime_error("lost session"); }
	if(!life) { life.reset(new msgpack::zone()); }

	vrefbuffer* buf = life->allocate<vrefbuffer>();
	msgid_t msgid = pack(*buf, method, params);

	mp::pthread_scoped_lock lk(m_callbacks_mutex);
	m_callbacks[msgid] =  // FIXME m_callbacks.insert
		callback_entry(callback, life, timeout_steps);

	mp::pthread_scoped_lock blk(m_binds_mutex);
	if(m_binds.empty()) {
		LOG_TRACE("push pending queue ",m_pending_queue.size()+1);
		m_pending_queue.push_back(buf);
		// FIXME clear pending queue if it is too big
		// FIXME or throw exception

	} else {
		// ad-hoc load balancing
		m_binds[m_msgid_rr % m_binds.size()]->send_datav(
				buf, NULL, NULL);
	}
}

inline void session::cancel_pendings()
{
	mp::pthread_scoped_lock lk(m_callbacks_mutex);
	m_pending_queue.clear();
}


}  // namespace rpc

#endif /* rpc/session_impl.h */

