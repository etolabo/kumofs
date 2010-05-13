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
#include "server/framework.h"
#include "server/mod_control.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <sys/resource.h>

namespace kumo {
namespace server {


void mod_control_t::create_backup(
		shared_zone life,
		std::string suffix,
		rpc::weak_responder response)
{
	std::string dst = share->cfg_db_backup_basename() + suffix;
	LOG_INFO("create backup: ",dst);

	try {
		share->db().backup(dst.c_str());
		response.result(true);

	} catch (storage_backup_error& e) {
		LOG_ERROR("backup failed ",dst,": ",e.what());
		response.error(true);
	}
}

RPC_IMPL(mod_control_t, CreateBackup, req, z, response)
{
	shared_zone life(z.release());
	wavy::submit(&mod_control_t::create_backup, this,
			life, req.param().suffix, response);
}


RPC_IMPL(mod_control_t, GetStatus, req, z, response)
{
	LOG_DEBUG("GetStatus");

	switch((status_type)req.param().command) {
	case STAT_PID:
		response.result((uint32_t)getpid());
		break;

	case STAT_UPTIME:
		response.result(time(NULL) - share->stat_start_time());
		break;

	case STAT_TIME:
		response.result((uint64_t)time(NULL));
		break;

	case STAT_VERSION:
#ifdef VERSION
#ifdef REVISION
		response.result(std::string(VERSION " revision " REVISION));
#else
		response.result(std::string(VERSION));
#endif
#else
		response.result(std::string("unknown"));
#endif
		break;

	case STAT_CMD_GET:
		response.result(share->stat_num_get());
		break;

	case STAT_CMD_SET:
		response.result(share->stat_num_set());
		break;

	case STAT_CMD_DELETE:
		response.result(share->stat_num_delete());
		break;

	case STAT_DB_ITEMS:
		response.result( share->db().rnum() );
		break;

	case STAT_CLOCKTIME:
		response.result( net->clocktime_now() );
		break;

	case STAT_RHS:
		{
			HashSpace::Seed sd;
			{
				pthread_scoped_rdlock rhlk(share->rhs_mutex());
				sd = share->rhs();
			}
			response.result(sd);
		}
		break;

	case STAT_WHS:
		{
			HashSpace::Seed sd;
			{
				pthread_scoped_rdlock whlk(share->whs_mutex());
				sd = share->whs();
			}
			response.result(sd);
		}
		break;

	default:
		response.result(msgpack::type::nil());
		break;
	}
}


RPC_IMPL(mod_control_t, SetConfig, req, z, response)
{
	LOG_DEBUG("SetConfig");

	switch((config_type)req.param().command) {
	case CONF_TCP_NODELAY:
		{
			bool enable = req.param().arg.as<bool>();

			struct rlimit rbuf;
			if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
				throw mp::system_error(errno, "getrlimit() failed");
			}
			int maxfd = rbuf.rlim_cur;

			int on = (enable) ? 1 : 0;
			unsigned int s = 0;
			for(int i=0; i < maxfd; ++i) {
				if(::setsockopt(i, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) == 0) {
					++s;
				}
			}

			response.result(s);
		}
	}
}


}  // namespace server
}  // namespace kumo

