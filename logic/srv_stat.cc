#include "logic/srv_impl.h"
#include <sys/types.h>
#include <unistd.h>

#include "config.h"

namespace kumo {


RPC_FUNC(GetStatus, from, response, z, param)
try {
	LOG_DEBUG("GetStatus");

	switch((protocol::status_type)param.command()) {
	case  protocol::STAT_PID:
		response.result((uint32_t)getpid());
		break;

	case  protocol::STAT_UPTIME:
		response.result(time(NULL) - m_start_time);
		break;

	case  protocol::STAT_TIME:
		response.result((uint64_t)time(NULL));
		break;

	case  protocol::STAT_VERSION:
		response.result(std::string(VERSION));
		break;

	case  protocol::STAT_CMD_GET:
		response.result(m_stat_num_get);
		break;

	case  protocol::STAT_CMD_SET:
		response.result(m_stat_num_set);
		break;

	case  protocol::STAT_CMD_DELETE:
		response.result(m_stat_num_delete);
		break;

	case  protocol::STAT_DB_ITEMS:
		{
			uint64_t num;
			{
				pthread_scoped_rdlock dblk(m_db.mutex());
				num = m_db.rnum();
			}
			response.result(num);
		}
		break;

	default:
		response.result(msgpack::type::nil());
		break;
	}
}
RPC_CATCH(GetStatus, response)


}  // namespace kumo

