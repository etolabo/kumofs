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
#ifndef KUMO_GATE_MEMCACHE_BINARY_H__
#define KUMO_GATE_MEMCACHE_BINARY_H__

#include "gate/interface.h"

namespace kumo {


class MemcacheBinary : public gate::gate {
public:
	MemcacheBinary(int lsock, bool save_flag, bool save_exptime);
	~MemcacheBinary();

	void run();

private:
	int m_lsock;

private:
	MemcacheBinary();
	MemcacheBinary(const MemcacheBinary&);
};


}  // namespace kumo

#endif /* gate/memcache_binary.h */

