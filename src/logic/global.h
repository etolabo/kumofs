#ifndef LOGIC_GLOBAL_H__
#define LOGIC_GLOBAL_H__

#include "log/mlogger.h"

#define NUM_REPLICATION 2

#ifndef MANAGER_DEFAULT_PORT
#define MANAGER_DEFAULT_PORT  19700
#endif

#ifndef SERVER_DEFAULT_PORT
#define SERVER_DEFAULT_PORT  19800
#endif

#ifndef SERVER_STREAM_DEFAULT_PORT
#define SERVER_STREAM_DEFAULT_PORT  19900
#endif

#ifndef CONTROL_DEFAULT_PORT
#define CONTROL_DEFAULT_PORT  19750
#endif

// FIXME VERSION
#define VERSION "0.1.0"

#endif /* logic/global.h */

