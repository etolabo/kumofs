#ifndef RPC_SESSION_IMPL_H__
#define RPC_SESSION_IMPL_H__

namespace rpc {


inline void basic_session::callback_table::insert(
		msgid_t msgid, const callback_entry& entry)
{
	pthread_scoped_lock lk(m_callbacks_mutex[msgid % PARTITION_NUM]);
	std::pair<callbacks_t::iterator, bool> pair =
		m_callbacks[msgid % PARTITION_NUM].insert(
				callbacks_t::value_type(msgid, entry));
	if(!pair.second) {
		pair.first->second = entry;
	}
}


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


inline unsigned short basic_session::increment_connect_retried_count()
	{ return ++m_connect_retried_count; } // FIXME atomic?

inline unsigned short basic_session::connect_retried_count()
	{ return m_connect_retried_count; }


inline session::session(session_manager* mgr) :
	basic_session(mgr)
{ }

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


template <typename Message>
msgid_t basic_session::pack(vrefbuffer& buffer, Message& param)
{
	msgid_t msgid = __sync_add_and_fetch(&m_msgid_rr, 1);
	rpc_request<Message> msgreq(typename Message::method(), param, msgid);
	msgpack::pack(buffer, msgreq);
	return msgid;
}


template <typename Message>
void basic_session::call(
		Message& param,
		shared_zone life, callback_t callback,
		unsigned short timeout_steps)
{
	LOG_DEBUG("send request method=",Message::method::id);
	if(is_lost()) { throw std::runtime_error("lost session"); }
	//if(!life) { life.reset(new msgpack::zone()); }

	std::auto_ptr<vrefbuffer> buf(new vrefbuffer());
	msgid_t msgid = pack(*buf, param);

	pthread_scoped_lock blk(m_binds_mutex);
	if(m_binds.empty()) {
		//throw std::runtime_error("session not bound");
		// FIXME XXX forget the error for robustness.
		// FIXME XXX wait timeout:
		m_cbtable.insert(msgid, callback_entry(callback, life, timeout_steps));
		buf.release();

	} else {
		m_cbtable.insert(msgid, callback_entry(callback, life, timeout_steps));
		// ad-hoc load balancing
		m_binds[m_msgid_rr % m_binds.size()]->send_datav(buf.get(),
				&mp::object_delete<vrefbuffer>, buf.get());
		buf.release();
	}
}


template <typename Message>
void session::call(
		Message& param,
		shared_zone life, callback_t callback,
		unsigned short timeout_steps)
{
	LOG_DEBUG("send request method=",Message::method::id);
	if(is_lost()) { throw std::runtime_error("lost session"); }
	//if(!life) { life.reset(new msgpack::zone()); }

	std::auto_ptr<vrefbuffer> buf(new vrefbuffer());
	msgid_t msgid = pack(*buf, param);

	m_cbtable.insert(msgid, callback_entry(callback, life, timeout_steps));

	pthread_scoped_lock blk(m_binds_mutex);
	if(m_binds.empty()) {
		{
			pthread_scoped_lock plk(m_pending_queue_mutex);
			LOG_TRACE("push pending queue ",m_pending_queue.size()+1);
			m_pending_queue.push_back(buf.get());
		}
		buf.release();
		// FIXME clear pending queue if it is too big
		// FIXME or throw exception

	} else {
		// ad-hoc load balancing
		m_binds[m_msgid_rr % m_binds.size()]->send_datav(buf.get(),
				&mp::object_delete<vrefbuffer>, buf.get());
		buf.release();
	}
}

inline void session::cancel_pendings()
{
	pthread_scoped_lock lk(m_pending_queue_mutex);
	clear_pending_queue(m_pending_queue);
}

inline void session::clear_pending_queue(pending_queue_t& queue)
{
	for(pending_queue_t::iterator it(queue.begin()),
			it_end(queue.end()); it != it_end; ++it) {
		delete *it;
	}
	queue.clear();
}


}  // namespace rpc

#endif /* rpc/session_impl.h */

