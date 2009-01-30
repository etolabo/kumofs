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

#ifndef MP_WAVY_EDGE_KQUEUE_H__
#define MP_WAVY_EDGE_KQUEUE_H__

#include "mp/exception.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

namespace mp {
namespace wavy {


static const short EVEDGE_READ  = EVFILT_READ;
static const short EVEDGE_WRITE = EVFILT_WRITE;


class edge {
public:
	edge() : m_kq(kqueue())
	{
		if(m_kq < 0) {
			throw system_error(errno, "failed to initialize kqueue");
		}
	}

	~edge()
	{
		::close(m_kq);
	}

	int add_notify(int fd, short event)
	{
		struct kevent kev;
		EV_SET(&kev, fd, event, EV_ADD|EV_ONESHOT, 0, 0, NULL);
		return kevent(m_kq, &kev, 1, NULL, 0, NULL);
	}

	int shot_reactivate(int fd, short event)
	{
		return add_notify(fd, event);
	}

	int shot_remove(int fd, short event)
	{ return 0; }

	int remove(int fd, short event)
	{
		struct kevent kev;
		EV_SET(&kev, fd, event, EV_DELETE, 0, 0, NULL);
		return kevent(m_kq, &kev, 1, NULL, 0, NULL);
	}

	struct backlog {
		backlog()
		{
			buf = (struct kevent*)::calloc(
					sizeof(struct kevent),
					MP_WAVY_EDGE_BACKLOG_SIZE);
			if(!buf) { throw std::bad_alloc(); }
		}

		~backlog()
		{
			::free(buf);
		}

		int operator[] (int n) const
		{
			return buf[n].ident;
		}

	private:
		struct kevent* buf;
		friend class edge;
		backlog(const backlog&);
	};

	int wait(backlog* result)
	{
		return kevent(m_kq, NULL, 0, result->buf,
				MP_WAVY_EDGE_BACKLOG_SIZE, NULL);
	}

	int wait(backlog* result, int timeout_msec)
	{
		struct timespec ts;
		ts.tv_sec  = timeout_msec / 1000;
		ts.tv_nsec = (timeout_msec % 1000) * 1000000;
		return kevent(m_kq, NULL, 0, result->buf,
				MP_WAVY_EDGE_BACKLOG_SIZE, &ts);
	}

private:
	int m_kq;

private:
	edge(const edge&);
};


}  // namespace wavy
}  // namespace mp

#endif /* wavy_edge_kqueue.h */

