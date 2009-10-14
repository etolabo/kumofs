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
#ifndef RPC_WEAK_RESPONDER_FWD_H__
#define RPC_WEAK_RESPONDER_FWD_H__

#include "rpc/types.h"

namespace rpc {


class weak_responder {
public:
	weak_responder(basic_weak_session s, msgid_t msgid);

	~weak_responder();

	template <typename Result>
	void result(Result res);

	template <typename Result>
	void result(Result res, auto_zone& z);

	template <typename Result>
	void result(Result res, shared_zone& life);

	template <typename Error>
	void error(Error err);

	template <typename Error>
	void error(Error err, auto_zone& z);

	template <typename Error>
	void error(Error err, shared_zone& life);

	void null();

private:
	template <typename Result, typename Error>
	void call(Result& res, Error& err);

	template <typename Result, typename Error>
	void call(Result& res, Error& err, auto_zone& z);

	template <typename Result, typename Error>
	void call(Result& res, Error& err, shared_zone& life);

	template <typename Result, typename Error, typename ZoneType>
	void call_impl(Result& res, Error& err, ZoneType& life);

private:
	basic_weak_session m_session;
	const msgid_t m_msgid;

private:
	weak_responder();
};


}  // namespace rpc

#endif /* rpc/weak_responder_fwd.h */

