#include "logic/srv_impl.h"

namespace kumo {


CLUSTER_FUNC(CreateBackup, from, response, life, param)
try {
	std::string dst = m_cfg_db_backup_basename + param.suffix();
	m_db.copy(dst.c_str());
	response.result(true);
}
RPC_CATCH(CreateBackup, response)


}  // namespace kumo

