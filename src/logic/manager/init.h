#ifndef MANAGER_INIT_H__
#define MANAGER_INIT_H__

#include "manager/framework.h"

namespace kumo {
namespace manager {


template <typename Config>
framework::framework(const Config& cfg) :
	cluster_logic<framework>(
			cfg.rthreads, cfg.wthreads,
			ROLE_MANAGER,
			cfg.cluster_addr,
			cfg.connect_timeout_msec,
			cfg.connect_retry_limit)
{
	LOG_INFO("start manager ",addr());
	LOGPACK("SM",2,
			"time", time(NULL),
			"addr", cfg.cluster_addr,
			"Padd", share->partner());
	listen_cluster(cfg.cluster_lsock);  // cluster_logic
	m_control_framework.listen_control(cfg.ctlsock_lsock);
	start_timeout_step(cfg.clock_interval_usec);  // rpc_server
	start_keepalive(cfg.keepalive_interval_usec);  // cluster_logic
}

template <typename Config>
resource::resource(const Config& cfg) :
	m_partner(cfg.partner),
	m_cfg_auto_replace(cfg.auto_replace),
	m_cfg_replace_delay_clocks(cfg.replace_delay_clocks)
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

