#include "logic/srv_impl.h"
#include <sys/types.h>
#include <unistd.h>

#include "config.h"

namespace kumo {


RPC_FUNC(GetStatus, from, response, z, param)
try {
	LOG_DEBUG("GetStatus");

	switch(param.command()) {
	case  0:  // pid
		response.result((uint32_t)getpid());
		break;

	case  1:  // uptime
		response.result(time(NULL) - m_start_time);
		break;

	case  2:  // server time
		response.result((uint64_t)time(NULL));
		break;

	case  3:  // version
		response.result(std::string(VERSION));
		break;

	case  4:    // cmd_get
		response.result(m_stat_num_get);
		break;

	case  5:    // cmd_set
		response.result(m_stat_num_set);
		break;

	case  6:    // cmd_delete
		response.result(m_stat_num_delete);
		break;

	case  7:    // num items
		{
			uint64_t num;
			{
				pthread_scoped_rdlock dblk(m_db.mutex());
				num = m_db.rnum();
			}
			response.result(num);
		}
		break;

	case  8:    // num connections
		{
			// FIXME extreamly rough value
			int pair[2];
			int ret = ::pipe(pair);
			if(ret < 0) {
				response.error(msgpack::type::nil());
			}
			::close(pair[0]);
			::close(pair[1]);
			response.result(pair[0]);
		}
		break;

	case  9:    // bytes_read
		// XXX not implemented
	case 10:    // bytes_write
		// XXX not implemented
	default:
		response.result(msgpack::type::nil());
		break;
	}
}
RPC_CATCH(GetStatus, response)


}  // namespace kumo

