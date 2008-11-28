#ifndef RPC_CLIENT_TMPL_H__
#define RPC_CLIENT_TMPL_H__

#ifndef RPC_CLIENT_H__
#error "don't include this file directly"
#endif

namespace rpc {


template <typename Transport, typename Session>
client<Transport, Session>::client(unsigned short connect_timeout_steps,
		unsigned int reconnect_timeout_msec) :
	m_connect_timeout_steps(connect_timeout_steps),
	m_reconnect_timeout_msec(reconnect_timeout_msec)
{
	if(m_connect_timeout_steps == 0) {
		// m_connect_timeout_steps == 0 may cause infinity loop in step_timeout
		// step_timeout: session_lost -> connect_session -> session_lost -> ...
		m_connect_timeout_steps = 1;
	}
}

template <typename Transport, typename Session>
client<Transport, Session>::~client() { }


template <typename Transport, typename Session>
typename client<Transport, Session>::shared_session
client<Transport, Session>::get_session(const address& addr)
{
	LOG_TRACE("get session ",addr);
	mp::pthread_scoped_lock lk(m_session_mutex);
	std::pair<typename sessions_t::iterator, typename sessions_t::iterator> pair =
		m_sessions.equal_range(addr);

	shared_session s;
	while(pair.first != pair.second) {
		s = pair.first->second.lock();
		if(s && !s->is_lost()) { return s; }
		++pair.first;
		//m_sessions.erase(pair.first++);  // lock avoid
	}
	LOG_TRACE("no session exist, connecting ",addr);
	s.reset(new Session(this));
	connect_session(addr, s, &lk);
	return s;
}

template <typename Transport, typename Session>
typename client<Transport, Session>::shared_session
client<Transport, Session>::create_session(const address& addr)
{
	LOG_TRACE("create session ",addr);
	mp::pthread_scoped_lock lk(m_session_mutex);
	std::pair<typename sessions_t::iterator, typename sessions_t::iterator> pair =
		m_sessions.equal_range(addr);

	shared_session s;
	while(pair.first != pair.second) {
		s = pair.first->second.lock();
		if(s && !s->is_lost()) { return s; }
		++pair.first;
		//m_sessions.erase(pair.first++);  // lock avoid
	}
	s.reset(new Session(this));
	return s;
}

template <typename Transport, typename Session>
typename client<Transport, Session>::shared_session
client<Transport, Session>::add(int fd, const address& addr)
{
	shared_session s(new Session(this));
	mp::iothreads::add<Transport>(fd, s, this);
	mp::pthread_scoped_lock lk(m_session_mutex);
	m_sessions.insert( typename sessions_t::value_type(addr, s) );
	return s;
}


template <typename Transport, typename Session>
bool client<Transport, Session>::connect_session(
		const address& addr, shared_session& s,
		mp::pthread_scoped_lock* lk)
{
	if(!s->is_lost() && s->is_bound()) { return false; }
	//if(m_unbounds.find(addr) != m_unbounds.end()) { return false; }

	mp::pthread_scoped_lock xlk;
	if(!lk) { xlk.relock(m_session_mutex); }

	unbound_entry& entry(m_unbounds[addr]);
	entry.timeout_steps = m_connect_timeout_steps;
	entry.session = s;
	entry.addr = addr;

	m_sessions.insert( typename sessions_t::value_type(addr, weak_session(entry.session)) );

	// FIXME set m_reconnect_timeout_msec to
	//       mp::iothreads::connect
	LOG_INFO("connecting to ",addr);
	char addrbuf[addr.addrlen()];
	addr.getaddr((sockaddr*)&addrbuf);

	std::auto_ptr<connect_pack> asc(new connect_pack(this, addr));
	mp::iothreads::connect((sockaddr*)&addrbuf, sizeof(addrbuf),
			&connect_pack::callback, reinterpret_cast<void*>(asc.get()));
	asc.release();

	entry.session->increment_connect_retried_count();

	return true;
}


template <typename Transport, typename Session>
client<Transport, Session>::connect_pack::connect_pack(client* srv, const address& addr) :
	m_srv(srv), m_addr(addr) { }

template <typename Transport, typename Session>
void client<Transport, Session>::connect_pack::callback(void* data, int fd)
{
	std::auto_ptr<connect_pack> self(
			reinterpret_cast<connect_pack*>(data));
	if(fd >= 0) {
		self->m_srv->connect_success(self->m_addr, fd);
	} else {
		self->m_srv->connect_failed(self->m_addr, -fd);
	}
}

template <typename Transport, typename Session>
void client<Transport, Session>::connect_success(const address& addr, int fd)
{
	if(mp::iothreads::is_end()) { ::close(fd); return; }
	LOG_INFO("connect success: ",addr," fd(",fd,")");

	mp::pthread_scoped_lock lk(m_session_mutex);

	typename unbounds_t::iterator it = m_unbounds.find(addr);
	if(it == m_unbounds.end()) {
		::close(fd);
		return;
	}

	try {
		mp::iothreads::add<Transport>(fd,
				mp::static_pointer_cast<basic_session>(it->second.session),
				this);
	} catch (...) {
		::close(fd);
		m_unbounds.erase(it);
		throw;
	}
	m_unbounds.erase(it);
}

template <typename Transport, typename Session>
void client<Transport, Session>::connect_failed(const address& addr, int error)
{
	if(mp::iothreads::is_end()) { return; }
	LOG_INFO("connect failed: ",addr,": ",strerror(error));

	mp::pthread_scoped_lock lk(m_session_mutex);

	typename unbounds_t::iterator it(m_unbounds.find(addr));
	if(it == m_unbounds.end()) {
		return;
	}

#if 0
	// FIXME mp::iothreads::connect needs to be rewritten
	// retry only if timed out

	// retry connect
	char addrbuf[addr.addrlen()];
	addr.getaddr((sockaddr*)&addrbuf);

	std::auto_ptr<connect_pack> asc(new connect_pack(this, addr));
	mp::iothreads::connect((sockaddr*)&addrbuf, sizeof(addrbuf),
			&connect_pack::callback, reinterpret_cast<void*>(asc.get()));
	asc.release();
#else
	shared_session lock_avoid(it->second.session);
	m_unbounds.erase(it);
	lk.unlock();

	transport_lost(lock_avoid);
#endif
}

template <typename Transport, typename Session>
void client<Transport, Session>::dispatch_request(
		basic_shared_session& s, weak_responder response,
		method_id method, msgobj param, shared_zone& life)
{
	shared_session from = mp::static_pointer_cast<Session>(s);
	dispatch(from, response, method, param, life);
}


template <typename Transport, typename Session>
void client<Transport, Session>::step_timeout()
{
	LOG_TRACE("step timeout ",m_connect_timeout_steps);
	mp::pthread_scoped_lock lk(m_session_mutex);

	typedef std::vector<
		std::pair<address, shared_session>
		> lock_avoid_cons_t;
	lock_avoid_cons_t lock_avoid_cons;

	typedef std::vector<typename sessions_t::iterator> lock_avoid_ses_t;
	lock_avoid_ses_t lock_avoid_ses;

	for(typename unbounds_t::iterator it(m_unbounds.begin()), it_end(m_unbounds.end());
			it != it_end; ) {
		if(it->second.timeout_steps == 0) {
			address addr(it->second.addr);
			lock_avoid_cons.push_back( typename lock_avoid_cons_t::value_type(
						addr, it->second.session
						) );
			//connect_timeout(addr, it->second.session);
			m_unbounds.erase(it++);
			LOG_DEBUG("connect timed out: ",addr," ",m_unbounds.size());
		} else {
			--it->second.timeout_steps;
			it->second.session->step_timeout(
					basic_shared_session(it->second.session));
			++it;
		}
	}

	for(typename sessions_t::iterator it(m_sessions.begin()),
			it_end(m_sessions.end()); it != it_end; ) {
		shared_session s(it->second.lock());
		if(s && !s->is_lost()) {
			s->step_timeout(basic_shared_session(s));
			++it;
		} else {
			//lock_avoid_ses.push_back(it++);
			m_sessions.erase(it++);
		}
	}

	lk.unlock();
	for(typename lock_avoid_ses_t::iterator it(lock_avoid_ses.begin()),
			it_end(lock_avoid_ses.end()); it != it_end; ++it) {
		m_sessions.erase(*it);
	}
	for(typename lock_avoid_cons_t::iterator it(lock_avoid_cons.begin()),
			it_end(lock_avoid_cons.end()); it != it_end; ++it) {
		connect_timeout(it->first, it->second);
	}
}


template <typename Transport, typename Session>
void client<Transport, Session>::transport_lost_notify(basic_shared_session& s)
{
	shared_session x(mp::static_pointer_cast<Session>(s));
	transport_lost(x);
}


}  // namespace rpc

#endif /* rpc/client.h */

