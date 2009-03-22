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
	client_logic(unsigned short rthreads, unsigned short wthreads,
			unsigned int connect_timeout_msec,
			unsigned short connect_retry_limit) :
		rpc_server<Framework>(rthreads, wthreads),
		rpc::client(connect_timeout_msec, connect_retry_limit) { }
};


typedef mp::shared_ptr<rpc::session> shared_session;
typedef mp::weak_ptr<rpc::session> weak_session;


}  // namespace kumo

#endif /* logic/client_logic.h */

