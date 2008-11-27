#ifndef LOGIC_GLOBAL_H__
#define LOGIC_GLOBAL_H__

#include "log/mlogger.h"
#include "rpc/address.h"
#include <mp/functional.h>

#define NUM_REPLICATION 2
#define CLUSTER_DEFAULT_PORT  19800
#define CONTROL_DEFAULT_PORT  19799

namespace kumo {

using rpc::address;
using namespace mp::placeholders;

}  // namespace kumo

#endif /* logic/global.h */

