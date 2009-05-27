//
// mp::wavy
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

#ifndef MP_WAVY_SINGLETON_H__
#define MP_WAVY_SINGLETON_H__

#include "mp/wavy/core.h"
#include "mp/wavy/output.h"

namespace mp {
namespace wavy {


template <typename Instance>
struct singleton {

	typedef core::handler handler;
	typedef output::finalize_t finalize_t;
	typedef output::request request;

	static void initialize(size_t core_thread, size_t output_thread);

	static void add_core_thread(size_t num);
	static void add_output_thread(size_t num);

	static void join();
	static void detach();
	static void end();

	static void write(int fd, const char* buf, size_t buflen);
	static void writev(int fd, const iovec* vec, size_t veclen);

	static void write(int fd, const char* buf, size_t buflen, request req);
	static void write(int fd, const char* buf, size_t buflen, finalize_t finalize, void* user);
	static void writev(int fd, const iovec* vec, size_t veclen, request req);
	static void writev(int fd, const iovec* vec, size_t veclen, finalize_t finalize, void* user);

	static void writev(int fd, const iovec* bufvec, const request* reqvec, size_t veclen);


	typedef core::connect_callback_t connect_callback_t;
	static void connect(
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t callback);


	typedef core::listen_callback_t listen_callback_t;
	static void listen(int lsock, listen_callback_t callback);


	typedef core::timer_callback_t timer_callback_t;
	static void timer(const timespec* interval, timer_callback_t callback);


	template <typename Handler>
	static Handler* add(int fd);
MP_ARGS_BEGIN
	template <typename Handler, MP_ARGS_TEMPLATE>
	static Handler* add(int fd, MP_ARGS_PARAMS);
MP_ARGS_END

	template <typename F>
	static void submit(F f);
MP_ARGS_BEGIN
	template <typename F, MP_ARGS_TEMPLATE>
	static void submit(F f, MP_ARGS_PARAMS);
MP_ARGS_END

private:
	static core* s_core;
	static output* s_output;

	singleton();
};

template <typename Instance>
core* singleton<Instance>::s_core;

template <typename Instance>
output* singleton<Instance>::s_output;

template <typename Instance>
void singleton<Instance>::initialize(size_t core_thread, size_t output_thread)
{
	s_core = new core();
	s_output = new output();
	add_core_thread(core_thread);
	add_output_thread(output_thread);
}

template <typename Instance>
void singleton<Instance>::add_core_thread(size_t num)
	{ s_core->add_thread(num); }

template <typename Instance>
void singleton<Instance>::add_output_thread(size_t num)
	{ s_output->add_thread(num); }

template <typename Instance>
void singleton<Instance>::join()
{
	s_core->join();
	s_output->join();
}

template <typename Instance>
void singleton<Instance>::detach()
{
	s_core->detach();
	s_output->detach();
}

template <typename Instance>
void singleton<Instance>::end()
{
	s_core->end();
	s_output->end();
}

template <typename Instance>
inline void singleton<Instance>::write(int fd, const char* buf, size_t buflen)
	{ s_output->write(fd, buf, buflen); }

template <typename Instance>
inline void singleton<Instance>::writev(int fd, const iovec* vec, size_t veclen)
	{ s_output->writev(fd, vec, veclen); }

template <typename Instance>
inline void singleton<Instance>::write(int fd, const char* buf, size_t buflen, request req)
	{ s_output->write(fd, buf, buflen, req); }

template <typename Instance>
inline void singleton<Instance>::write(int fd, const char* buf, size_t buflen, finalize_t finalize, void* user)
	{ s_output->write(fd, buf, buflen, finalize, user); }

template <typename Instance>
inline void singleton<Instance>::writev(int fd, const iovec* vec, size_t veclen, request req)
	{ s_output->writev(fd, vec, veclen, req); }

template <typename Instance>
inline void singleton<Instance>::writev(int fd, const iovec* vec, size_t veclen, finalize_t finalize, void* user)
	{ s_output->writev(fd, vec, veclen, finalize, user); }

template <typename Instance>
inline void singleton<Instance>::writev(int fd, const iovec* bufvec, const request* reqvec, size_t veclen)
	{ s_output->writev(fd, bufvec, reqvec, veclen); }


template <typename Instance>
inline void singleton<Instance>::connect(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		int timeout_msec, connect_callback_t callback)
{
	s_core->connect(socket_family, socket_type, protocol,
			addr, addrlen, timeout_msec, callback);
}


template <typename Instance>
inline void singleton<Instance>::listen(int lsock, listen_callback_t callback)
	{ s_core->listen(lsock, callback); }


template <typename Instance>
inline void singleton<Instance>::timer(
		const timespec* interval, timer_callback_t callback)
	{ s_core->timer(interval, callback); }


template <typename Instance>
template <typename Handler>
inline Handler* singleton<Instance>::add(int fd)
	{ return s_core->add<Handler>(fd); }
MP_ARGS_BEGIN
template <typename Instance>
template <typename Handler, MP_ARGS_TEMPLATE>
inline Handler* singleton<Instance>::add(int fd, MP_ARGS_PARAMS)
	{ return s_core->add<Handler, MP_ARGS_TYPES>(fd, MP_ARGS_FUNC); }
MP_ARGS_END

template <typename Instance>
template <typename F>
inline void singleton<Instance>::submit(F f)
	{ s_core->submit<F>(f); }
MP_ARGS_BEGIN
template <typename Instance>
template <typename F, MP_ARGS_TEMPLATE>
inline void singleton<Instance>::submit(F f, MP_ARGS_PARAMS)
	{ s_core->submit<F, MP_ARGS_TYPES>(f, MP_ARGS_FUNC); }
MP_ARGS_END


}  // namespace wavy
}  // namespace mp

#endif /* mp/wavy/singleton.h */

