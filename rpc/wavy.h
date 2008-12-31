#ifndef RPC_WAVY_H__
#define RPC_WAVY_H__

#include <mp/wavy.h>
#include <mp/wavy/singleton.h>
#include <memory>

namespace rpc {

struct rpc_wavy { };

typedef mp::wavy::singleton<rpc_wavy> wavy;


}  // namespace rpc

#endif /* rpc/wavy.h */

