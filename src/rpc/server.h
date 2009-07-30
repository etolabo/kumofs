//
// kumofs
//
// Copyright (C) 2009 Etolabo Corp.
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
#ifndef RPC_SERVER_H__
#define RPC_SERVER_H__

#include "rpc/rpc.h"
#include "log/mlogger.h"  // FIXME
#include <mp/pthread.h>
#include <algorithm>
#include <map>

namespace rpc {


class server;

class peer : public basic_session {
public:
	peer(session_manager* mgr = NULL);
};

typedef mp::shared_ptr<peer> shared_peer;
typedef mp::weak_ptr<peer> weak_peer;


class server : public session_manager, public transport_manager {
public:
	typedef shared_peer shared_session;
	typedef weak_peer weak_session;

	server();

	virtual ~server();

	virtual void dispatch(
			shared_peer from, weak_responder response,
			method_id method, msgobj param, auto_zone z) = 0;

public:
	// step timeout count.
	void step_timeout();

	// add accepted connection
	shared_peer accepted(int fd);

	// apply function to all connected sessions.
	// F is required to implement
	// void operator() (shared_peer);
	template <typename F>
	void for_each_peer(F f);

protected:
	mp::pthread_mutex m_peers_mutex;
	typedef std::map<void*, basic_weak_session> peers_t;
	peers_t m_peers;

public:
	virtual void dispatch_request(
			basic_shared_session& s, weak_responder response,
			method_id method, msgobj param, auto_zone z);

	virtual void transport_lost_notify(basic_shared_session& s);

private:
	server(const server&);
};


inline peer::peer(session_manager* mgr) :
	basic_session(mgr) { }


namespace detail {
	template <typename F>
	struct server_each_peer {
		server_each_peer(F f) : m(f) { }
		inline void operator() (std::pair<void* const, basic_weak_session>& x)
		{
			basic_shared_session s(x.second.lock());
			if(s && !s->is_lost()) {
				m(mp::static_pointer_cast<peer>(s));
			}
		}
	private:
		F m;
		server_each_peer();
	};
}  // namespace detail

template <typename F>
void server::for_each_peer(F f)
{
	pthread_scoped_lock lk(m_peers_mutex);
	detail::server_each_peer<F> e(f);
	std::for_each(m_peers.begin(), m_peers.end(), e);
}


}  // namespace rpc

#endif /* rpc/client.h */

