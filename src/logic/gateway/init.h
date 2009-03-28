#ifndef GATEWAY_INIT_H__
#define GATEWAY_INIT_H__

#include "gateway/framework.h"

namespace kumo {
namespace gateway {


template <typename Config>
framework::framework(const Config& cfg) :
	client_logic<framework>(
			cfg.connect_timeout_msec,
			cfg.connect_retry_limit)
{ }

template <typename Config>
void framework::run(const Config& cfg)
{
	init_wavy(cfg.rthreads, cfg.wthreads);  // wavy_server
	start_timeout_step(cfg.clock_interval_usec);  // rpc_server
	start_keepalive(cfg.keepalive_interval_usec);  // rpc_server
	scope_proto_network().renew_hash_space();
	TLOGPACK("SW",2,
			"mgr1", share->manager1(),
			"mgr2", share->manager2());
}

template <typename Config>
resource::resource(const Config& cfg) :
	m_manager1(cfg.manager1),
	m_manager2(cfg.manager2),
	m_cfg_async_replicate_set(cfg.async_replicate_set),
	m_cfg_async_replicate_delete(cfg.async_replicate_delete),
	m_cfg_get_retry_num(cfg.get_retry_num),
	m_cfg_set_retry_num(cfg.set_retry_num),
	m_cfg_delete_retry_num(cfg.delete_retry_num),
	m_cfg_renew_threshold(cfg.renew_threshold),
	m_error_count(0)
{ }

template <typename Config>
static void init(const Config& cfg)
{
	share.reset(new resource(cfg));
	net.reset(new framework(cfg));
}


}  // namespace kumo
}  // namespace gateway

#endif /* gateway/init.h */

