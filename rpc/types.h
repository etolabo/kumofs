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


typedef uint16_t method_id;
typedef uint32_t msgid_t;
typedef uint8_t role_type;


class transport;
class session;
class basic_transport;
class basic_session;


typedef mp::shared_ptr<basic_session> basic_shared_session;
typedef mp::weak_ptr<basic_session> basic_weak_session;

typedef mp::function<void (basic_shared_session, msgobj, msgobj, shared_zone)> callback_t;


using mp::pthread_scoped_lock;
using mp::pthread_scoped_rdlock;
using mp::pthread_scoped_wrlock;


}  // namespace rpc

#endif /* rpc/types.h */
