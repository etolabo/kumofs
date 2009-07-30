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
#ifndef RPC_RESPONDER_IMPL_H__
#define RPC_RESPONDER_IMPL_H__

#include <mp/object_callback.h>
#include "rpc/vrefbuffer.h"
#include "rpc/wavy.h"

namespace rpc {


inline responder::responder(int fd, msgid_t msgid) :
	m_fd(fd), m_msgid(msgid) { }

inline responder::~responder() { }

template <typename Result>
void responder::result(Result res)
{
	msgpack::type::nil err;
	call(res, err);
}

template <typename Result>
void responder::result(Result res, auto_zone z)
{
	msgpack::type::nil err;
	call(res, err, z);
}

template <typename Error>
void responder::error(Error err)
{
	msgpack::type::nil res;
	call(res, err);
}

template <typename Error>
void responder::error(Error err, auto_zone z)
{
	msgpack::type::nil res;
	call(res, err, z);
}

inline void responder::null()
{
	msgpack::type::nil res;
	msgpack::type::nil err;
	call(res, err);
}

template <typename Result, typename Error>
inline void responder::call(Result& res, Error& err)
{
	msgpack::sbuffer buf;  // FIXME use vrefbuffer?
	rpc_response<Result&, Error> msgres(res, err, m_msgid);
	msgpack::pack(buf, msgres);

	wavy::request req(&::free, buf.data());
	wavy::write(m_fd, buf.data(), buf.size(), req);
	buf.release();
}

template <typename Result, typename Error>
inline void responder::call(Result& res, Error& err, auto_zone z)
{
	vrefbuffer* buf = z->allocate<vrefbuffer>();
	rpc_response<Result&, Error> msgres(res, err, m_msgid);
	msgpack::pack(*buf, msgres);

	wavy::request req(&mp::object_delete<msgpack::zone>, z.get());
	wavy::writev(m_fd, buf->vector(), buf->vector_size(), req);
	z.release();
}

inline void responder::send_response(const char* buf, size_t buflen, auto_zone z)
{
	wavy::request req(&mp::object_delete<msgpack::zone>, z.get());
	wavy::write(m_fd, buf, buflen, req);
	z.release();
}

inline void responder::send_responsev(const struct iovec* vb, size_t count, auto_zone z)
{
	wavy::request req(&mp::object_delete<msgpack::zone>, z.get());
	wavy::writev(m_fd, vb, count, req);
	z.release();
}


}  // namespace rpc

#endif /* rpc/responder_impl.h */

