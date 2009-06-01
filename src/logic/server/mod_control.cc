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


RPC_IMPL(mod_control_t, CreateBackup, req, z, response)
{
	std::string dst = share->cfg_db_backup_basename() + req.param().suffix;
	LOG_INFO("create backup: ",dst);

	try {
		share->db().backup(dst.c_str());
		response.result(true);

	} catch (storage_backup_error& e) {
		LOG_ERROR("backup failed ",dst,": ",e.what());
		response.error(true);
	}
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
		response.result(std::string(VERSION));
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

