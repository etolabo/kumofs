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
#ifndef LOGIC_CLIENT_LOGIC__
#define LOGIC_CLIENT_LOGIC__

#include "rpc/client.h"
#include "logic/rpc_server.h"
#include "logic/hash.h"
#include "logic/clock.h"
#include "logic/global.h"

namespace kumo {


template <typename Framework>
class client_logic : public rpc_server<Framework>, public rpc::client {
public:
	client_logic(
			unsigned int connect_timeout_msec,
			unsigned short connect_retry_limit) :
		rpc::client(connect_timeout_msec, connect_retry_limit) { }
};


typedef mp::shared_ptr<rpc::session> shared_session;
typedef mp::weak_ptr<rpc::session> weak_session;


}  // namespace kumo

#endif /* logic/client_logic.h */

