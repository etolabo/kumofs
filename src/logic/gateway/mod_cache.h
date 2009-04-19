#ifndef GATEWAY_MOD_CACHE_H__
#define GATEWAY_MOD_CACHE_H__

#include <tcutil.h>
#include <tcadb.h>
#include "logic/msgtype.h"

namespace kumo {
namespace gateway {


class mod_cache_t {
public:
	mod_cache_t();
	~mod_cache_t();

public:
	void init(const char* name);

public:
	bool get(const msgtype::DBKey& key, msgtype::DBValue* result_val,
			msgpack::zone* z)
	{
		if(!m_db) { return false; }
		return get_real(key, result_val, z);
	}

	void update(const msgtype::DBKey& key, const msgtype::DBValue& val)
	{
		if(!m_db) { return; }
		return update_real(key, val);
	}

private:
	bool get_real(const msgtype::DBKey& key, msgtype::DBValue* result_val,
			msgpack::zone* z);

	void update_real(const msgtype::DBKey& key, const msgtype::DBValue& val);

public:
	TCADB* m_db;
};


}  // namespace gateway
}  // namespace kumo

#endif /* gateway/mod_cache.h */

