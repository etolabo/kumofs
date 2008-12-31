#ifndef LOGIC_GW_IMPL_H__
#define LOGIC_GW_IMPL_H__

#include "logic/gw.h"

namespace kumo {


#define BIND_RESPONSE(FUNC, ...) \
	mp::bind(&Gateway::FUNC, this, _1, _2, _3, _4, ##__VA_ARGS__)

#define RPC_FUNC(NAME, from, response, z, param) \
	RPC_IMPL(Gateway, NAME, from, response, z, param)

#define RPC_REPLY(NAME, from, res, err, life, ...) \
	RPC_REPLY_IMPL(Gateway, NAME, from, res, err, life, ##__VA_ARGS__)


}  // namespace kumo

#endif /* logic/gw_impl.h */

