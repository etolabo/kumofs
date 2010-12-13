//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
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
#ifndef SERVER_INIT_H__
#define SERVER_INIT_H__

#include "server/framework.h"

namespace kumo {
namespace server {


template <typename Config>
framework::framework(const Config& cfg) :
	cluster_logic<framework>(
			ROLE_SERVER,
			cfg.cluster_addr,
			cfg.connect_timeout_msec,
			cfg.connect_retry_limit),
	mod_replace_stream(cfg.stream_addr)
{ }

template <typename Config>
void framework::run(const Config& cfg)
{
	init_wavy(cfg.rthreads, cfg.wthreads);  // wavy_server
	listen_cluster(cfg.cluster_lsock);  // cluster_logic
	start_timeout_step(cfg.clock_interval_usec);  // rpc_server
	start_keepalive(cfg.keepalive_interval_usec);  // rpc_server
	mod_replace_stream.init_stream(cfg.stream_lsock);
	LOG_INFO("start server ",addr());
	TLOGPACK("SS",2,
			"addr", cfg.cluster_addr,
			"db",   cfg.dbpath,
			"mgr1", share->manager1(),
			"mgr2", share->manager2(),
			"sadd", cfg.stream_addr,
			"tmpd", share->cfg_offer_tmpdir(),
			"bkup", share->cfg_db_backup_basename());
}

template <typename Config>
resource::resource(const Config& cfg) :
	m_db(*cfg.db),
	m_manager1(cfg.manager1),
	m_manager2(cfg.manager2),

	m_cfg_offer_tmpdir(cfg.offer_tmpdir),
	m_cfg_db_backup_basename(cfg.db_backup_basename),
	m_cfg_replicate_set_retry_num(cfg.replicate_set_retry_num),
	m_cfg_replicate_delete_retry_num(cfg.replicate_delete_retry_num),
	m_cfg_replace_set_limit_mem(cfg.replace_set_limit_mem),

	m_stat_start_time(time(NULL)),
	m_stat_num_get(0),
	m_stat_num_set(0),
	m_stat_num_delete(0)
{ }

template <typename Config>
static void init(const Config& cfg)
{
	share.reset(new resource(cfg));
	net.reset(new framework(cfg));
}



}  // namespace server
}  // namespace kumo

#endif /* server/init.h */

