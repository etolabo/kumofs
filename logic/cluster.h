#ifndef LOGIC_CLUSTER_H__
#define LOGIC_CLUSTER_H__

#include "logic/rpc.h"
#include "rpc/cluster.h"
#include <mp/utility.h>

namespace kumo {

using rpc::role_type;
using rpc::shared_node;

template <typename Logic>
class ClusterBase : public RPCBase<Logic> {
public:
	typedef Logic ServerClass;

	template <typename Config>
	ClusterBase(Config& cfg) :
		RPCBase<Logic>(cfg) { }

	~ClusterBase() { }

	void listen_cluster(int fd)
	{
		mp::iothreads::listen(fd, ClusterBase<Logic>::checked_accepted, this);
	}

private:
	static void checked_accepted(void* data, int fd)
	{
		ServerClass* self = static_cast<ServerClass*>(data);
		if(fd < 0) {
			LOG_FATAL("accept failed: ",strerror(-fd));
			self->signal_end(SIGTERM);
			return;
		}
		mp::set_nonblock(fd);
		self->accepted(fd);
	}
};


}  // namespace kumo

#endif /* logic/cluster.h */

