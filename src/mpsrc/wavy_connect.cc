//
// mp::wavy::core::connect
//
// Copyright (C) 2008-2009 FURUHASHI Sadayuki
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

#include "wavy_core.h"
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>

namespace mp {
namespace wavy {


class core::impl::connect_thread {
public:
	struct pack {
		int        socket_family;
		int        socket_type;
		int        protocol;
		socklen_t  addrlen;
		int        timeout_msec;
		core*      c;
		sockaddr   addr[0];
	};

	connect_thread(core* c,
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t& callback) :
		m((pack*)::malloc(sizeof(pack)+addrlen)),
		m_callback(callback)
	{
		if(!m) { throw std::bad_alloc(); }
		m->socket_family = socket_family;
		m->socket_type   = socket_type;
		m->protocol      = protocol;
		m->addrlen       = addrlen;
		m->timeout_msec  = timeout_msec;
		m->c             = c;
		::memcpy(m->addr, addr, addrlen);
	}

	void operator() ()
	{
		int err = 0;
		int fd = ::socket(m->socket_family, m->socket_type, m->protocol);
		if(fd < 0) {
			err = errno;
			goto out;
		}

		if(::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
			goto errno_error;
		}

		if(::connect(fd, m->addr, m->addrlen) >= 0) {
			// connect success
			goto out;
		}

		if(errno != EINPROGRESS) {
			goto errno_error;
		}

		while(true) {
			struct pollfd pf = {fd, POLLOUT, 0};
			int ret = ::poll(&pf, 1, m->timeout_msec);
			if(ret < 0) {
				if(errno == EINTR) { continue; }
				goto errno_error;
			}

			if(ret == 0) {
				errno = ETIMEDOUT;
				goto specific_error;
			}

			{
				int value = 0;
				int len = sizeof(value);
				if(::getsockopt(fd, SOL_SOCKET, SO_ERROR,
						&value, (socklen_t*)&len) < 0) {
					goto errno_error;
				}
				if(value != 0) {
					err = value;
					goto specific_error;
				}
				goto out;
			}
		}

	errno_error:
		err = errno;

	specific_error:
		::close(fd);
		fd = -1;

	out:
		try {
			m->c->submit(m_callback, fd, err);
		} catch (...) {
			::free(m);
			throw;
		}

		::free(m);
		return;
	}

private:
	pack* m;
	connect_callback_t m_callback;
};


void core::connect(int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		int timeout_msec, connect_callback_t callback)
{
	impl::connect_thread t(this,
			socket_family, socket_type, protocol,
			addr, addrlen, timeout_msec, callback);
	submit(t);
}


}  // namespace wavy
}  // namespace mp

