//
// kumofs
//
// Copyright (C) 2009 Etolabo Corp.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
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

