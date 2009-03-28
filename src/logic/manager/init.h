#ifndef MANAGER_INIT_H__
#define MANAGER_INIT_H__

#include "manager/framework.h"

namespace kumo {
namespace manager {


template <typename Config>
framework::framework(const Config& cfg) :
	cluster_logic<framework>(
			ROLE_MANAGER,
			cfg.cluster_addr,
			cfg.connect_timeout_msec,
			cfg.connect_retry_limit)
{ }

template <typename Config>
void framework::run(const Config& cfg)
{
	init_wavy(cfg.rthreads, cfg.wthreads);  // wavy_server
	listen_cluster(cfg.cluster_lsock);  // cluster_logic
	start_timeout_step(cfg.clock_interval_usec);  // rpc_server
	start_keepalive(cfg.keepalive_interval_usec);  // rpc_server
	LOG_INFO("start manager ",addr());
	TLOGPACK("SM",2,
			"addr", cfg.cluster_addr,
			"Padd", share->partner());
}

template <typename Config>
resource::resource(const Config& cfg) :
	m_partner(cfg.partner),
	m_cfg_auto_replace(cfg.auto_replace),
	m_cfg_replace_delay_seconds(cfg.replace_delay_seconds)
{ }

template <typename Config>
static void init(const Config& cfg)
{
	share.reset(new resource(cfg));
	net.reset(new framework(cfg));
}


}  // namespace manager
}  // namespace kumo

#endif /* manager/init.h */

