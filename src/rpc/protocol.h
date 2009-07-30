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
#ifndef RPC_PROTOCOL_H__
#define RPC_PROTOCOL_H__

#include <msgpack.hpp>
#include "rpc/types.h"
#include "rpc/address.h"

namespace rpc {


namespace protocol {
	using msgpack::define;
	using msgpack::type::tuple;
	using msgpack::type::raw_ref;

	static const int TRANSPORT_LOST_ERROR = 1;
	static const int NODE_LOST_ERROR      = 2;
	static const int TIMEOUT_ERROR        = 3;
	static const int UNKNOWN_ERROR        = 4;
	static const int PROTOCOL_ERROR       = 5;
	static const int SERVER_ERROR         = 6;
}


typedef uint16_t protocol_id;
typedef uint16_t version_id;

struct method_id {
	method_id() { }
	method_id(uint32_t id) : m(id) { }

	uint32_t get() const { return m; }

	uint16_t protocol() const { return m & 0xffff; }
	uint16_t version()  const { return m >> 16; }

	void msgpack_unpack(uint32_t id) { m = id; }

	template <typename Packer>
	void msgpack_pack(Packer& pk) const { pk.pack(m); }

private:
	uint32_t m;
};


template <uint16_t Protocol, uint16_t Version>
struct method : public method_id {
	static const uint16_t protocol = Protocol;
	static const uint16_t version  = Version;
	static const uint32_t id = Version << 16 | Protocol;
	method() : method_id(id) { }
};


typedef uint16_t rpc_type_t;

namespace rpc_type {
	static const rpc_type_t MESSAGE_REQUEST  = 0;
	static const rpc_type_t MESSAGE_RESPONSE = 1;
	static const rpc_type_t CLUSTER_INIT     = 2;
}  // namespace rpc_type


struct rpc_message : msgpack::define< msgpack::type::tuple<rpc_type_t> > {
	rpc_message() { }
	bool is_request()  const { return get<0>() == rpc_type::MESSAGE_REQUEST; }
	//bool is_response() const { return get<0>() == rpc_type::MESSAGE_RESPONSE; }
	bool is_cluster_init() const { return get<0>() == rpc_type::CLUSTER_INIT; }
};


template <typename Parameter>
struct rpc_request : msgpack::define<
			msgpack::type::tuple<rpc_type_t, msgid_t, method_id, Parameter> > {

	typedef rpc_request<Parameter> this_t;

	rpc_request() { }

	rpc_request(
			method_id method,
			typename msgpack::type::tuple_type<Parameter>::transparent_reference params,
			msgid_t msgid) :
		this_t::define_type(typename this_t::msgpack_type(
					rpc_type::MESSAGE_REQUEST,
					msgid,
					method,
					params
					)) { }

	msgid_t   msgid()  const { return this_t::msgpack_type::template get<1>(); }

	method_id method() const { return this_t::msgpack_type::template get<2>(); }

	typename msgpack::type::tuple_type<Parameter>::const_reference
	param()  const { return this_t::msgpack_type::template get<3>(); }
};


template <typename Result, typename Error>
struct rpc_response : msgpack::define<
			msgpack::type::tuple<rpc_type_t, msgid_t, Error, Result> > {

	typedef rpc_response<Result, Error> this_t;

	rpc_response() { }

	rpc_response(
			typename msgpack::type::tuple_type<Result>::transparent_reference res,
			typename msgpack::type::tuple_type<Error >::transparent_reference err,
			msgid_t msgid) :
		this_t::define_type(typename this_t::msgpack_type(
					rpc_type::MESSAGE_RESPONSE,
					msgid,
					err,
					res
					)) { }

	msgid_t msgid()  const { return this_t::msgpack_type::template get<1>(); }

	typename msgpack::type::tuple_type<Error>::const_reference
	error()  const { return this_t::msgpack_type::template get<2>(); }

	typename msgpack::type::tuple_type<Result>::const_reference
	result() const { return this_t::msgpack_type::template get<3>(); }
};


struct rpc_initmsg : msgpack::define<
		msgpack::type::tuple<rpc_type_t, msgpack::type::raw_ref, role_type> > {

	typedef rpc_initmsg this_t;

	rpc_initmsg() { }

	rpc_initmsg(
			const address& addr,
			role_type id) :
		this_t::define_type(msgpack_type(
					rpc_type::CLUSTER_INIT,
					msgpack::type::raw_ref(addr.dump(), addr.dump_size()),
					id
					)) { }

	address addr() const { return address(get<1>().ptr, get<1>().size); }

	role_type role_id() const { return get<2>(); }
};


}  // namespace rpc

#endif /* rpc/protocol.h */

