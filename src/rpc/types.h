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
#ifndef RPC_TYPES_H__
#define RPC_TYPES_H__

#include <msgpack.hpp>
#include <mp/memory.h>
#include <mp/functional.h>
#include <mp/pthread.h>

namespace rpc {


typedef msgpack::object msgobj;
typedef std::auto_ptr<msgpack::zone> auto_zone;
typedef mp::shared_ptr<msgpack::zone> shared_zone;


typedef uint32_t msgid_t;
typedef uint8_t role_type;


class transport;
class session;
class basic_transport;
class basic_session;


typedef mp::shared_ptr<basic_session> basic_shared_session;
typedef mp::weak_ptr<basic_session> basic_weak_session;

typedef mp::function<void (basic_shared_session, msgobj, msgobj, auto_zone)> callback_t;


using mp::pthread_scoped_lock;
using mp::pthread_scoped_rdlock;
using mp::pthread_scoped_wrlock;


}  // namespace rpc

#endif /* rpc/types.h */
