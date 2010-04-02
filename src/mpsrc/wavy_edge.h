#ifndef MP_WAVY_EDGE_H__
#define MP_WAVY_EDGE_H__

#include "mp/pp.h"

#ifndef MP_WAVY_EDGE
#  if   defined(HAVE_SYS_EPOLL_H)
#    define MP_WAVY_EDGE epoll
#  elif defined(HAVE_SYS_EVENT_H)
#    define MP_WAVY_EDGE kqueue
#  elif defined(HAVE_PORT_H)
#    define MP_WAVY_EDGE eventport
#  else
#    if   defined(__linux__)
#      define MP_WAVY_EDGE epoll
#    elif defined(__APPLE__) && defined(__MACH__)
#      define MP_WAVY_EDGE kqueue
#    elif defined(__FreeBSD__) || defined(__NetBSD__)
#      define MP_WAVY_EDGE kqueue
#    elif defined(__sun__)
#      define MP_WAVY_EDGE eventport
#    else
#      error this system is not supported.
#    endif
#  endif
#endif

#define MP_WAVY_EDGE_HEADER(sys) \
	MP_PP_HEADER(mpsrc, wavy_edge_, sys, )

#ifndef MP_WAVY_EDGE_BACKLOG_SIZE
#define MP_WAVY_EDGE_BACKLOG_SIZE 256
#endif

#include MP_WAVY_EDGE_HEADER(MP_WAVY_EDGE)

#endif /* wavy_edge.h */

