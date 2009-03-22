#ifndef RPC_SESSION_IMPL_H__
#define RPC_SESSION_IMPL_H__

namespace rpc {


inline bool basic_session::is_bound() const
{
	return !m_binds.empty();
}

inline bool basic_session::is_lost() const
{
	return m_lost;
}

inline void basic_session::revive()
{
	m_lost = false;
}

inline unsigned short basic_session::increment_connect_retried_count()
{
	return ++m_connect_retried_count;  // FIXME atomic?
}

inline unsigned short basic_session::connect_retried_count()
{
	return m_connect_retried_count;
}


inline session::session(session_manager* mgr) :
	basic_session(mgr) { }

inline session::~session() { cancel_pendings(); }


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
inline msgid_t basic_session::pack(vrefbuffer& buffer, Message& param)
{
	msgid_t msgid = __sync_add_and_fetch(&m_msgid_rr, 1);
	rpc_request<Message> msgreq(typename Message::method(), param, msgid);
	msgpack::pack(buffer, msgreq);
	return msgid;
}


template <typename Message>
inline void basic_session::call(
		Message& param,
		shared_zone life, callback_t callback,
		unsigned short timeout_steps)
{
	LOG_DEBUG("send request method=",Message::method::id);

	std::auto_ptr<vrefbuffer> buffer(new vrefbuffer());
	msgid_t msgid = pack(*buffer, param);

	call_real(msgid, buffer, life, callback, timeout_steps);
}


template <typename Message>
inline void session::call(
		Message& param,
		shared_zone life, callback_t callback,
		unsigned short timeout_steps)
{
	LOG_DEBUG("send request method=",Message::method::id);

	std::auto_ptr<vrefbuffer> buffer(new vrefbuffer());
	msgid_t msgid = pack(*buffer, param);

	call_real(msgid, buffer, life, callback, timeout_steps);
}


}  // namespace rpc

#endif /* rpc/session_impl.h */

