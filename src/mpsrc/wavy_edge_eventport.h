//
// mp::wavy::eventport
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

#ifndef MP_WAVY_EDGE_EVENTPORT_H__
#define MP_WAVY_EDGE_EVENTPORT_H__

#include "mp/exception.h"
#include <stdlib.h>
#include <port.h>
#include <poll.h>

namespace mp {
namespace wavy {


static const short EVEDGE_READ  = POLLIN;
static const short EVEDGE_WRITE = POLLOUT;


class edge {
public:
	edge() : m_port(port_create())
	{
		if(m_port < 0) {
			throw system_error(errno, "failed to initialize event port");
		}
	}

	~edge()
	{
		::close(m_port);
	}

	int add_notify(int fd, short event)
	{
		return port_associate(m_port, PORT_SOURCE_FD,
				fd, event, NULL);
	}

	int shot_reactivate(int fd, short event)
	{
		return port_associate(m_port, PORT_SOURCE_FD,
				fd, event, NULL);
	}

	int shot_remove(int fd, short event)
	{
		return 0;
	}

	int remove(int fd, short event)
	{
		return port_dissociate(m_port, PORT_SOURCE_FD, fd);
	}

	struct backlog {
		backlog()
		{
			buf = (port_event_t*)::calloc(
					sizeof(port_event_t),
					MP_WAVY_EDGE_BACKLOG_SIZE);
			if(!buf) { throw std::bad_alloc(); }
		}

		~backlog()
		{
			::free(buf);
		}

		int operator[] (int n)
		{
			return buf[n].portev_object;
		}

	private:
		port_event_t* buf;
		friend class edge;
		backlog(const backlog&);
	};

	int wait(backlog* result)
	{
		uint_t num = MP_WAVY_EDGE_BACKLOG_SIZE;
		if(port_getn(m_port, result->buf, 0, &num, NULL) < 0) {
			return -1;
		}
		if(num == 0) num = 1;
		if(num > MP_WAVY_EDGE_BACKLOG_SIZE) num = MP_WAVY_EDGE_BACKLOG_SIZE;
		if(port_getn(m_port, result->buf, MP_WAVY_EDGE_BACKLOG_SIZE, &num, NULL) < 0) {
			return -1;
		}
		return num;
	}

	int wait(backlog* result, int timeout_msec)
	{
		struct timespec ts;
		ts.tv_sec  = timeout_msec / 1000;
		ts.tv_nsec = (timeout_msec % 1000) * 1000000;
		uint_t num = MP_WAVY_EDGE_BACKLOG_SIZE;
		if(port_getn(m_port, result->buf, 0, &num, &ts) < 0) {
			return -1;
		}
		if(num == 0) num = 1;
		if(num > MP_WAVY_EDGE_BACKLOG_SIZE) num = MP_WAVY_EDGE_BACKLOG_SIZE;
		if(port_getn(m_port, result->buf, MP_WAVY_EDGE_BACKLOG_SIZE, &num, &ts) < 0) {
			if(errno == ETIME) { return 0; }
			return -1;
		}
		return num;
	}

private:
	int m_port;

private:
	edge(const edge&);
};


}  // namespace wavy
}  // namespace mp

#endif /* wavy_edge_eventport.h */

