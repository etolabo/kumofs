//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#ifndef RPC_CLIENT_TMPL_H__
#define RPC_CLIENT_TMPL_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

namespace rpc {


template <typename Transport, typename Session>
client_tmpl<Transport, Session>::client_tmpl(
		unsigned int connect_timeout_msec,
		unsigned short connect_retry_limit) :
	m_connect_timeout_msec(connect_timeout_msec),
	m_connect_retry_limit(connect_retry_limit)
{ }

template <typename Transport, typename Session>
client_tmpl<Transport, Session>::~client_tmpl() { }


template <typename Transport, typename Session>
template <bool CONNECT>
typename client_tmpl<Transport, Session>::shared_session
client_tmpl<Transport, Session>::get_session_impl(const address& addr)
{
	shared_session s;

	{
		pthread_scoped_rdlock rdlk(m_sessions_rwlock);

		std::pair<typename sessions_t::iterator, typename sessions_t::iterator> pair =
			m_sessions.equal_range(addr);

		while(pair.first != pair.second) {
			s = pair.first->second.lock();
			if(s && !s->is_lost()) { return s; }
			++pair.first;
			//m_sessions.erase(pair.first++);
		}
	}

	// ほとんどの場合rwlockだけでヒットする
	{
		pthread_scoped_wrlock wrlk(m_sessions_rwlock);

		std::pair<typename sessions_t::iterator, typename sessions_t::iterator> pair =
			m_sessions.equal_range(addr);

		while(pair.first != pair.second) {
			s = pair.first->second.lock();
			if(s && !s->is_lost()) { return s; }
			//++pair.first;
			m_sessions.erase(pair.first++);
		}

		LOG_TRACE("no session exist, creating ",addr);
		s.reset(new Session(this));
		m_sessions.insert( typename sessions_t::value_type(
					addr, weak_session(s)) );
	}

	if(!CONNECT) {
		return s;
	}

	async_connect(addr, s);
	return s;
}


template <typename Transport, typename Session>
typename client_tmpl<Transport, Session>::shared_session
inline client_tmpl<Transport, Session>::get_session(const address& addr)
{
	LOG_TRACE("get session ",addr);
	return get_session_impl<true>(addr);
}


template <typename Transport, typename Session>
typename client_tmpl<Transport, Session>::shared_session
inline client_tmpl<Transport, Session>::create_session(const address& addr)
{
	LOG_TRACE("create session ",addr);
	return get_session_impl<false>(addr);
}


template <typename Transport, typename Session>
typename client_tmpl<Transport, Session>::shared_session
client_tmpl<Transport, Session>::add(int fd, const address& addr)
{
	shared_session s(new Session(this));
	wavy::add<Transport>(fd, s, this);

	pthread_scoped_wrlock lk(m_sessions_rwlock);
	m_sessions.insert( typename sessions_t::value_type(addr, s) );
	return s;
}


namespace detail {
	template <typename Session, typename F>
	struct client_tmpl_each_session {
		client_tmpl_each_session(F f) : m(f) { }
		inline void operator() (std::pair<const address, mp::weak_ptr<Session> >& x)
		{
			mp::shared_ptr<Session> s(x.second.lock());
			if(s && !s->is_lost()) {
				m(s);
			}
		}
	private:
		F m;
		client_tmpl_each_session();
	};
}  // namespace detail

template <typename Transport, typename Session>
template <typename F>
void client_tmpl<Transport, Session>::for_each_session(F f)
{
	pthread_scoped_rdlock rdlk(m_sessions_rwlock);
	detail::client_tmpl_each_session<Session, F> e(f);
	std::for_each(m_sessions.begin(), m_sessions.end(), e);
}



template <typename Transport, typename Session>
bool client_tmpl<Transport, Session>::async_connect(
		const address& addr, shared_session& s)
{
	// rough check
	if(!s->is_lost() && s->is_bound()) { return false; }

	LOG_INFO("connecting to ",addr);
	char addrbuf[addr.addrlen()];
	addr.getaddr((sockaddr*)&addrbuf);

	using namespace mp::placeholders;
#ifdef KUMO_IPV6
	wavy::connect(PF_INET6, SOCK_STREAM, 0,
#else
	wavy::connect(PF_INET, SOCK_STREAM, 0,
#endif
			(sockaddr*)addrbuf, sizeof(addrbuf),
			m_connect_timeout_msec,
			mp::bind(
				&client_tmpl<Transport, Session>::connect_callback,
				this, addr, s, _1, _2));

	s->increment_connect_retried_count();
	return true;
}

template <typename Transport, typename Session>
void client_tmpl<Transport, Session>::connect_callback(
		address addr, shared_session s, int fd, int err)
{
	if(fd >= 0) {
#ifndef NO_TCP_NODELAY
		// XXX
		int on = 1;
		::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));  // ignore error
#endif
#ifndef NO_SO_LINGER
		struct linger opt = {0, 0};
		::setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&opt, sizeof(opt));  // ignore error
#endif
		LOG_INFO("connect success ",addr," fd(",fd,")");
		try {
			basic_shared_session bs(mp::static_pointer_cast<basic_session>(s));
			wavy::add<Transport>(fd, bs, this);
		} catch (...) {
			::close(fd);
			throw;
		}
		return;
	}

//error:
	LOG_INFO("connect failed ",addr,": ",strerror(err));
	if(s->connect_retried_count() > m_connect_retry_limit) {
		connect_failed(s, addr, err);
		return;
	}

	// retry connect
	// FIXME: retry only when err == ETIMEDOUT?
	async_connect(addr, s);
}


template <typename Transport, typename Session>
void client_tmpl<Transport, Session>::transport_lost(shared_session& s)
{
	msgpack::object res;
	res.type = msgpack::type::NIL;
	msgpack::object err;
	err.type = msgpack::type::POSITIVE_INTEGER;
	err.via.u64 = protocol::NODE_LOST_ERROR;

	basic_shared_session bs = mp::static_pointer_cast<basic_session>(s);
	s->force_lost(bs, res, err);
}


template <typename Transport, typename Session>
inline void client_tmpl<Transport, Session>::dispatch_request(
		basic_shared_session& s, weak_responder response,
		method_id method, msgobj param, auto_zone z)
{
	dispatch(mp::static_pointer_cast<Session>(s),
			response, method, param, z);
}


template <typename Transport, typename Session>
void client_tmpl<Transport, Session>::step_timeout()
{
	LOG_TRACE("step timeout ...");

	pthread_scoped_wrlock lk(m_sessions_rwlock);
	for(typename sessions_t::iterator it(m_sessions.begin()),
			it_end(m_sessions.end()); it != it_end; ) {
		shared_session s(it->second.lock());
		if(s && !s->is_lost()) {
			wavy::submit(&basic_session::step_timeout,
					s,
					mp::static_pointer_cast<Session>(s));
			++it;
		} else {
			m_sessions.erase(it++);
		}
	}

	LOG_TRACE("step timeout done");
}


template <typename Transport, typename Session>
void client_tmpl<Transport, Session>::transport_lost_notify(basic_shared_session& s)
{
	shared_session x(mp::static_pointer_cast<Session>(s));
	transport_lost(x);
}


}  // namespace rpc

#endif /* rpc/client_tmpl.h */

