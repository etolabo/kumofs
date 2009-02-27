#include "logic/srv_impl.h"

namespace kumo {


CLUSTER_FUNC(CreateBackup, from, response, z, param)
try {
	std::string dst = m_cfg_db_backup_basename + param.suffix();
	LOG_INFO("create backup: ",dst);

	m_db.backup(dst.c_str());

	response.result(true);
}
RPC_CATCH(CreateBackup, response)


}  // namespace kumo

