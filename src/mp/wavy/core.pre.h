//
// mp::wavy::core
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

#ifndef MP_WAVY_CORE_H__
#define MP_WAVY_CORE_H__

#include "mp/functional.h"
#include "mp/memory.h"
#include "mp/pthread.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>
#include <queue>

namespace mp {
namespace wavy {


class core {
public:
	core();
	~core();

	void add_thread(size_t num);

	void end();
	bool is_end() const;

	void join();
	void detach();


	struct handler {
		handler(int fd) : m_fd(fd) { }
		virtual ~handler() { ::close(m_fd); }
		virtual void read_event() = 0;

		int fd() const { return m_fd; }

		template <typename IMPL>
		shared_ptr<IMPL> shared_self()
		{
			return static_pointer_cast<IMPL>(*m_shared_self);
		}

	private:
		int m_fd;
		shared_ptr<handler>* m_shared_self;
		friend class core;
	};

	typedef function<void (int fd, int err)> connect_callback_t;
	void connect(int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t callback);


	typedef function<void (int fd, int err)> listen_callback_t;
	void listen(int lsock, listen_callback_t callback);


	typedef function<void ()> timer_callback_t;
	void timer(const timespec* interval, timer_callback_t callback);


	template <typename Handler>
	Handler* add(int fd);
MP_ARGS_BEGIN
	template <typename Handler, MP_ARGS_TEMPLATE>
	Handler* add(int fd, MP_ARGS_PARAMS);
MP_ARGS_END

	template <typename F>
	void submit(F f);
MP_ARGS_BEGIN
	template <typename F, MP_ARGS_TEMPLATE>
	void submit(F f, MP_ARGS_PARAMS);
MP_ARGS_END

private:
	void add_impl(int fd, handler* newh);

	typedef function<void ()> task_t;
	void submit_impl(task_t f);

private:
	class impl;
	const std::auto_ptr<impl> m_impl;

	core(const core&);
};

typedef core::handler handler;


template <typename Handler>
Handler* core::add(int fd)
	{ Handler* h = new Handler(fd); add_impl(fd, h); return h; }
MP_ARGS_BEGIN
template <typename Handler, MP_ARGS_TEMPLATE>
Handler* core::add(int fd, MP_ARGS_PARAMS)
	{ Handler* h = new Handler(fd, MP_ARGS_FUNC); add_impl(fd, h); return h; }
MP_ARGS_END

template <typename F>
inline void core::submit(F f)
	{ submit_impl(task_t(f)); }
MP_ARGS_BEGIN
template <typename F, MP_ARGS_TEMPLATE>
inline void core::submit(F f, MP_ARGS_PARAMS)
	{ submit_impl(bind(f, MP_ARGS_FUNC)); }
MP_ARGS_END


}  // namespace wavy
}  // namespace mp

#endif /* mp/wavy/core.h */

