#ifndef RPC_MESSAGE_H__
#define RPC_MESSAGE_H__

#include <msgpack.hpp>
#include "rpc/address.h"

namespace rpc {


typedef uint16_t method_id;
typedef uint32_t msgid_t;
typedef uint8_t role_type;


struct rpc_message : msgpack::define<
			msgpack::type::tuple<bool, msgid_t, msgpack::object, msgpack::object> > {
	rpc_message() { }
	bool is_request() const { return get<0>() == true; }
};


template <typename Parameter>
struct rpc_request : msgpack::define<
			msgpack::type::tuple<bool, msgid_t, method_id, Parameter> > {

	typedef rpc_request<Parameter> this_t;
	rpc_request() { }

	rpc_request(rpc_message& msg) {
		this_t::msgpack_type::template get<0>() = msg.get<0>();
		this_t::msgpack_type::template get<1>() = msg.get<1>();
		this_t::msgpack_type::template get<2>() = msg.get<2>().convert();
		this_t::msgpack_type::template get<3>() = msg.get<3>().convert();
	}

	rpc_request(
			method_id method,
			typename msgpack::type::tuple_type<Parameter>::transparent_reference params,
			msgid_t msgid) :
		this_t::define_type(typename this_t::msgpack_type(
					true, msgid, method, params )) { }

	msgid_t   msgid()  const { return this_t::msgpack_type::template get<1>(); }

	method_id method() const { return this_t::msgpack_type::template get<2>(); }

	typename msgpack::type::tuple_type<Parameter>::const_reference
	param()  const { return this_t::msgpack_type::template get<3>(); }
};


template <typename Result, typename Error>
struct rpc_response : msgpack::define<
			msgpack::type::tuple<bool, msgid_t, Error, Result> > {

	typedef rpc_response<Result, Error> this_t;
	rpc_response() { }

	rpc_response(rpc_message& msg) {
		this_t::msgpack_type::template get<0>() = msg.get<0>();
		this_t::msgpack_type::template get<1>() = msg.get<1>();
		this_t::msgpack_type::template get<2>() = msg.get<2>().convert();
		this_t::msgpack_type::template get<3>() = msg.get<3>().convert();
	}

	rpc_response(
			typename msgpack::type::tuple_type<Result>::transparent_reference res,
			typename msgpack::type::tuple_type<Error >::transparent_reference err,
			msgid_t msgid) :
		this_t::define_type(typename this_t::msgpack_type(
					false, msgid, err, res )) { }

	msgid_t msgid()  const { return this_t::msgpack_type::template get<1>(); }

	typename msgpack::type::tuple_type<Error>::const_reference
	error()  const { return this_t::msgpack_type::template get<2>(); }

	typename msgpack::type::tuple_type<Result>::const_reference
	result() const { return this_t::msgpack_type::template get<3>(); }
};


struct rpc_initmsg : msgpack::define<
		msgpack::type::tuple<msgpack::type::raw_ref, role_type> > {
	rpc_initmsg() { }
	rpc_initmsg(const address& addr, role_type id) :
		define_type(msgpack_type(
					msgpack::type::raw_ref(addr.dump(), addr.dump_size()),
					id)) { }
	address addr() const { return address(get<0>().ptr, get<0>().size); }
	role_type role_id() const { return get<1>(); }
};


}  // namespace rpc

#endif /* rpc/message.h */

