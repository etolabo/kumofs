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
#ifndef GATEWAY_INTERFACE_H__
#define GATEWAY_INTERFACE_H__

#include "rpc/wavy.h"
#include "rpc/types.h"
#include <mp/utility.h>
#include <mp/memory.h>
#include <msgpack/zone.h>

namespace kumo {
namespace gate {


typedef rpc::wavy wavy;
using rpc::shared_zone;
using rpc::auto_zone;


class gate {
public:
	gate() { }
	virtual ~gate() { }
	virtual void run() = 0;
};


uint64_t stdhash(const char* key, size_t keylen);
void fatal_stop();


struct res_get {
	int error;
	const char* key;
	uint32_t keylen;
	uint64_t hash;

	char* val;
	uint32_t vallen;
	uint64_t clocktime;
};

typedef void (*callback_get)(void* user, res_get& res, auto_zone z);

struct req_get {
	req_get() { }

	const char* key;
	uint32_t keylen;
	uint64_t hash;

	shared_zone life;
	callback_get callback;
	void* user;

	void submit();
};


enum set_op_t {
	OP_SET       = 0,
	OP_SET_ASYNC = 1,
	OP_CAS       = 2,
	OP_APPEND    = 3,
	OP_PREPEND   = 4,
};


struct res_set {
	int error;

	const char* key;
	uint32_t keylen;
	uint64_t hash;

	const char* val;
	uint32_t vallen;
	uint64_t clocktime;

	bool cas_success;
};

typedef void (*callback_set)(void* user, res_set& res, auto_zone z);

struct req_set {
	req_set() : operation(OP_SET) { }

	const char* key;
	uint32_t keylen;
	uint64_t hash;

	const char* val;
	uint32_t vallen;

	set_op_t operation;
	uint64_t clocktime;

	shared_zone life;
	callback_set callback;
	void* user;

	void submit();
};


struct res_delete {
	int error;

	const char* key;
	uint32_t keylen;
	uint64_t hash;

	bool deleted;
};

typedef void (*callback_delete)(void* user, res_delete& res, auto_zone z);

struct req_delete {
	req_delete() : async(false) { }

	const char* key;
	uint32_t keylen;
	uint64_t hash;

	bool async;

	shared_zone life;
	callback_delete callback;
	void* user;

	void submit();
};


}  // namespace gate
}  // namespace kumo

#endif /* gate/interface.h */

