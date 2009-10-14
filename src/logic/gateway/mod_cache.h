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

