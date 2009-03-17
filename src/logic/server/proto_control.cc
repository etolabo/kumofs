#include "server/framework.h"
#include "server/proto_control.h"

namespace kumo {
namespace server {


RPC_IMPL(proto_control, CreateBackup_1, req, z, response)
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


RPC_IMPL(proto_control, GetStatus_1, req, z, response)
{
	LOG_DEBUG("GetStatus_1");

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

	default:
		response.result(msgpack::type::nil());
		break;
	}
}


}  // namespace server
}  // namespace kumo

