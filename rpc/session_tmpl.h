#ifndef RPC_SESSION_TMPL_H__
#define RPC_SESSION_TMPL_H__

#ifndef RPC_SESSION_H__
#error "don't include this file directly"
#endif

namespace rpc {


inline basic_session::basic_session(session_manager* mgr) :
	m_msgid_rr(0),  // FIXME randomize?
	m_lost(false),
	m_connect_retried_count(0),
	m_manager(mgr) { }


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


inline void basic_session::increment_connect_retried_count()
{
	++m_connect_retried_count;
}

inline unsigned short basic_session::connect_retried_count()
{
	return m_connect_retried_count;
}


inline session::session(session_manager* mgr) :
	basic_session(mgr) { }

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
		msgid_t msgid, auto_zone& z)
{
	shared_zone life(new mp::zone());
	life->push_finalizer(
			&mp::object_delete<msgpack::zone>,
			z.get());
	z.release();
	weak_responder response(s, msgid);
	get_manager()->dispatch_request(s, response, method, param, life);
}


template <typename Parameter>
msgid_t basic_session::pack(vrefbuffer& buffer, method_id method, Parameter& params)
{
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
}

inline void basic_session::set_callback(
		msgid_t msgid, callback_t callback,
		shared_zone life, unsigned short timeout_steps)
{
	m_callbacks[msgid] =
		callback_entry(callback, life, timeout_steps);
}


template <typename Parameter>
void basic_session::call(
		method_id method, Parameter& params,
		shared_zone& life, callback_t callback,
		unsigned short timeout_steps)
{
	if(m_lost) { throw std::runtime_error("lost session"); }

	mp::pthread_scoped_lock lk(m_binds_mutex);
	if(!is_bound()) {
		throw std::runtime_error("session not bound");
	}

	if(!life) { life.reset(new mp::zone()); }
	std::auto_ptr<vrefbuffer> buf(new vrefbuffer());
	msgid_t msgid = pack(*buf, method, params);

	void (*finalize)(void*) =
			&mp::iothreads::writer::finalize_delete<vrefbuffer>;
	send_bufferv(buf.get(), finalize,
			reinterpret_cast<void*>(buf.get()));
	buf.release();

	lk.unlock();
	set_callback(msgid, callback, life, timeout_steps);
}


template <typename Parameter>
void session::call(
		method_id method, Parameter& params,
		shared_zone& life, callback_t callback,
		unsigned short timeout_steps)
{
	if(m_lost) { throw std::runtime_error("lost session"); }

	if(!life) { life.reset(new mp::zone()); }

	mp::pthread_scoped_lock lk(m_binds_mutex);
	if(is_bound()) {
		std::auto_ptr<vrefbuffer> buf(new vrefbuffer());   // FIXME optimize
		msgid_t msgid = pack(*buf, method, params);

		void (*finalize)(void*) =
				&mp::iothreads::writer::finalize_delete<vrefbuffer>;
		send_bufferv(buf.get(), finalize,
			reinterpret_cast<void*>(buf.get()));
		buf.release();

		lk.unlock();
		set_callback(msgid, callback, life, timeout_steps);

	} else {
		vrefbuffer* buf = life->allocate<vrefbuffer>();
		msgid_t msgid = pack(*buf, method, params);

		m_pending_queue.push_back(buf);
		LOG_TRACE("push pending queue ",m_pending_queue.size());
		// FIXME clear pending queue if it is too big

		lk.unlock();
		set_callback(msgid, callback, life, timeout_steps);
	}
}

inline void session::cancel_pendings()
{
	mp::pthread_scoped_lock lk(m_binds_mutex);
	m_pending_queue.clear();
}




inline weak_responder::weak_responder(basic_weak_session s, msgid_t msgid) :
	m_session(s), m_msgid(msgid) { }

inline weak_responder::~weak_responder() { }

template <typename Result>
void weak_responder::result(Result res)
{
	msgpack::type::nil err;
	call(res, err);
}

template <typename Result>
void weak_responder::result(Result res, shared_zone& life)
{
	msgpack::type::nil err;
	call(res, err, life);
}

template <typename Error>
void weak_responder::error(Error err)
{
	msgpack::type::nil res;
	call(res, err);
}

template <typename Error>
void weak_responder::error(Error err, shared_zone& life)
{
	msgpack::type::nil res;
	call(res, err, life);
}

inline void weak_responder::null()
{
	msgpack::type::nil res;
	msgpack::type::nil err;
	call(res, err);
}


template <typename Result, typename Error>
void weak_responder::call(Result& res, Error& err)
{
	rpc::sbuffer buf;  // FIXME use vrefbuffer?
	rpc_response<Result&, Error> msgres(res, err, m_msgid);
	msgpack::pack(buf, msgres);

	basic_shared_session s(m_session.lock());
	if(!s) { throw std::runtime_error("session lost"); }

	s->send_response_data((const char*)buf.data(), buf.size(),
			&mp::iothreads::writer::finalize_free,
			reinterpret_cast<void*>(buf.data()));
	buf.release();
}



namespace detail {
	struct response_zone_keeper {
		response_zone_keeper(shared_zone z) : m(z) { }
		~response_zone_keeper() { }
		vrefbuffer buf;
	private:
		shared_zone m;
		response_zone_keeper();
		response_zone_keeper(const response_zone_keeper&);
	};
}


template <typename Result, typename Error>
void weak_responder::call(Result& res, Error& err, shared_zone& life)
{
	std::auto_ptr<detail::response_zone_keeper> zk(new detail::response_zone_keeper(life));

	rpc_response<Result&, Error> msgres(res, err, m_msgid);
	msgpack::pack(zk->buf, msgres);

	basic_shared_session s(m_session.lock());
	if(!s) { throw std::runtime_error("session lost"); }

	s->send_response_datav(&zk->buf,
			&mp::iothreads::writer::finalize_delete<detail::response_zone_keeper>,
			reinterpret_cast<void*>(zk.get()));
	zk.release();
}


}  // namespace rpc

#endif /* rpc/session_tmpl.h */

