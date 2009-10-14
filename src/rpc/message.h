//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
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
#ifndef RPC_MESSAGE_H__
#define RPC_MESSAGE_H__

#include <msgpack.hpp>

namespace rpc {


template <uint16_t Protocol, uint16_t Version, typename Session>
struct message {
	typedef Session session_type;
	typedef rpc::method<Protocol, Version> method;
};


}  // namespace rpc

#endif /* rpc/message.h */

