#ifndef RPC_TYPES_H__
#define RPC_TYPES_H__

#include <msgpack.hpp>
#include <mp/memory.h>
#include <mp/functional.h>

namespace rpc {


typedef uint16_t method_id;
typedef uint32_t msgid_t;
typedef uint8_t role_type;

typedef msgpack::object msgobj;

typedef std::auto_ptr<msgpack::zone> auto_zone;
typedef mp::shared_ptr<msgpack::zone> shared_zone;

class basic_transport;
class basic_session;
class session;

typedef mp::shared_ptr<basic_session> basic_shared_session;
typedef mp::weak_ptr<basic_session> basic_weak_session;

typedef mp::function<void (basic_shared_session, msgobj, msgobj, shared_zone)> callback_t;


}  // namespace rpc

#endif /* rpc/types.h */
