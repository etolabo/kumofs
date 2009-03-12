//
// mp::wavy::edge
//
// Copyright (C) 2008 FURUHASHI Sadayuki
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

#ifndef MP_WAVY_EDGE_EPOLL_H__
#define MP_WAVY_EDGE_EPOLL_H__

#include "mp/exception.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>

namespace mp {
namespace wavy {


static const short EVEDGE_READ  = EPOLLIN;
static const short EVEDGE_WRITE = EPOLLOUT;


class edge {
public:
	edge() : m_ep(epoll_create(MP_WAVY_EDGE_BACKLOG_SIZE))
	{
		if(m_ep < 0) {
			throw system_error(errno, "failed to initialize epoll");
		}
	}

	~edge()
	{
		::close(m_ep);
	}

	int add_notify(int fd, short event)
	{
		struct epoll_event ev;
		::memset(&ev, 0, sizeof(ev));  // FIXME
		ev.events = event | EPOLLONESHOT;
		ev.data.fd = fd;
		return epoll_ctl(m_ep, EPOLL_CTL_ADD, fd, &ev);
	}

	int shot_reactivate(int fd, short event)
	{
		struct epoll_event ev;
		::memset(&ev, 0, sizeof(ev));  // FIXME
		ev.events = event | EPOLLONESHOT;
		ev.data.fd = fd;
		return epoll_ctl(m_ep, EPOLL_CTL_MOD, fd, &ev);
	}

	int shot_remove(int fd, short event)
	{
		return epoll_ctl(m_ep, EPOLL_CTL_DEL, fd, NULL);
	}

	int remove(int fd, short event)
	{
		return epoll_ctl(m_ep, EPOLL_CTL_DEL, fd, NULL);
	}

	struct backlog {
		backlog()
		{
			buf = (struct epoll_event*)::calloc(
					sizeof(struct epoll_event),
					MP_WAVY_EDGE_BACKLOG_SIZE);
			if(!buf) { throw std::bad_alloc(); }
		}

		~backlog()
		{
			::free(buf);
		}

		int operator[] (int n)
		{
			return buf[n].data.fd;
		}

	private:
		struct epoll_event* buf;
		friend class edge;
		backlog(const backlog&);
	};

	int wait(backlog* result)
	{
		return wait(result, -1);
	}

	int wait(backlog* result, int timeout_msec)
	{
		return epoll_wait(m_ep, result->buf,
				MP_WAVY_EDGE_BACKLOG_SIZE, timeout_msec);
	}

private:
	int m_ep;

private:
	edge(const edge&);
};


}  // namespace wavy
}  // namespace mp

#endif /* wavy_edge_kqueue.h */

