//
// mp::wavy::core::listen
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

#include "wavy_core.h"
#include "mp/exception.h"

namespace mp {
namespace wavy {


class core::impl::listen_handler : public handler {
public:
	listen_handler(int fd, core* c, listen_callback_t callback) :
		handler(fd), m_core(c), m_callback(callback) { }

	~listen_handler() { }

	void read_event()
	{
		while(true) {
			int err = 0;
			int sock = ::accept(fd(), NULL, NULL);
			if(sock < 0) {
				if(errno == EAGAIN || errno == EINTR) {
					return;
				} else {
					err = errno;
				}
			} else if(sock == 0) {
				err = errno;
			}

			try {
				m_core->submit(m_callback, sock, err);
			} catch(...) { }

			if(err) {
				throw system_error(errno, "mp::wvy::accept: accept failed");
			}
		}
	}

private:
	core* m_core;
	listen_callback_t m_callback;
};


void core::listen(int lsock, listen_callback_t callback)
{
	add<impl::listen_handler>(lsock, this, callback);
}


}  // namespace wavy
}  // namespace mp

