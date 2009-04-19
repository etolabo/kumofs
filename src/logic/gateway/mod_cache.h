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
	char* get(const char* raw_key, uint32_t raw_keylen,
			uint32_t* result_raw_vallen, msgpack::zone* z);

	void update(const char* raw_key, uint32_t raw_keylen,
			const msgtype::DBValue& val);

public:
	TCADB* m_db;
};


}  // namespace gateway
}  // namespace kumo

#endif /* gateway/mod_cache.h */

