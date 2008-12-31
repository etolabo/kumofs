#ifndef LOGIC_CLUSTER_H__
#define LOGIC_CLUSTER_H__

#include "logic/rpc.h"
#include "rpc/cluster.h"
#include <mp/utility.h>

namespace kumo {

using rpc::role_type;

using rpc::weak_node;
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
		using namespace mp::placeholders;
		wavy::listen(fd, mp::bind(
					&ClusterBase<Logic>::checked_accepted, this,
					_1, _2));
	}

private:
	void checked_accepted(int fd, int err)
	{
		if(fd < 0) {
			LOG_FATAL("accept failed: ",strerror(err));
			RPCBase<Logic>::signal_end(SIGTERM);
			return;
		}
		LOG_DEBUG("accept cluster fd=",fd);
		static_cast<ServerClass*>(this)->rpc::cluster::accepted(fd);
	}
};


}  // namespace kumo

#endif /* logic/cluster.h */

