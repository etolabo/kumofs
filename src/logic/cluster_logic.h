#ifndef LOGIC_CLUSTER_LOGIC__
#define LOGIC_CLUSTER_LOGIC__

#include "rpc/cluster.h"
#include "logic/rpc_server.h"
#include "logic/hash.h"
#include "logic/clock.h"
#include "logic/global.h"
#include "logic/role.h"

namespace kumo {


using rpc::role_type;
using rpc::weak_node;
using rpc::shared_node;
using rpc::shared_peer;


template <typename Framework>
class cluster_logic : public rpc_server<Framework>, public rpc::cluster {
public:
	cluster_logic(
			role_type self_id,
			const address& self_addr,
			unsigned int connect_timeout_msec,
			unsigned short connect_retry_limit) :
		rpc::cluster(
				self_id,
				self_addr,
				connect_timeout_msec,
				connect_retry_limit) { }

protected:
	void listen_cluster(int fd)
	{
		using namespace mp::placeholders;
		wavy::listen(fd, mp::bind(
					&Framework::cluster_accepted, this,
					_1, _2));
	}

private:
	void cluster_accepted(int fd, int err)
	{
		if(fd < 0) {
			LOG_FATAL("accept failed: ",strerror(err));
			static_cast<Framework*>(this)->signal_end();
			return;
		}
		LOG_DEBUG("accept cluster fd=",fd);
		static_cast<Framework*>(this)->rpc::cluster::accepted(fd);
	}
};


#define REQUIRE_HSLK const pthread_scoped_lock& hslk
#define REQUIRE_RELK const pthread_scoped_lock& relk
#define REQUIRE_STLK const pthread_scoped_lock& stlk

#define REQUIRE_HSLK_RDLOCK const pthread_scoped_rdlock& hslk
#define REQUIRE_HSLK_WRLOCK const pthread_scoped_wrlock& hslk


}  // namespace kumo

#endif /* logic/cluster_logic.h */

