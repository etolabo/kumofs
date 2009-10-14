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
#ifndef GATEWAY_MOD_STORE_H__
#define GATEWAY_MOD_STORE_H__

#include "gate/interface.h"
#include "server/mod_store.h"

namespace kumo {
namespace gateway {


class mod_store_t {
public:
	mod_store_t();
	~mod_store_t();

public:
	void Get(gate::req_get& req);

	void Set(gate::req_set& req);

	void Delete(gate::req_delete& req);

private:
	RPC_REPLY_DECL(Get, from, res, err, z,
			rpc::retry<server::mod_store_t::Get>* retry,
			gate::callback_get callback, void* user);

	RPC_REPLY_DECL(GetIfModified, from, res, err, z,
			rpc::retry<server::mod_store_t::GetIfModified>* retry,
			gate::callback_get callback, void* user,
			msgtype::DBValue* cached_val);

	RPC_REPLY_DECL(Set, from, res, err, z,
			rpc::retry<server::mod_store_t::Set>* retry,
			gate::callback_set callback, void* user);

	RPC_REPLY_DECL(Delete, from, res, err, z,
			rpc::retry<server::mod_store_t::Delete>* retry,
			gate::callback_delete callback, void* user);
};


}  // namespace gateway
}  // namespace kumo

#endif /* gateway/mod_store.h */

