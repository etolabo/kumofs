#ifndef LOGIC_GLOBAL_H__
#define LOGIC_GLOBAL_H__

#include "log/mlogger.h"
#include "rpc/address.h"
#include <mp/functional.h>

#define NUM_REPLICATION 2

#ifndef MANAGER_DEFAULT_PORT
#define MANAGER_DEFAULT_PORT  19700
#endif

#ifndef SERVER_DEFAULT_PORT
#define SERVER_DEFAULT_PORT  19800
#endif

#ifndef CONTROL_DEFAULT_PORT
#define CONTROL_DEFAULT_PORT  19750
#endif

namespace kumo {

using rpc::address;
using namespace mp::placeholders;

}  // namespace kumo

#endif /* logic/global.h */

