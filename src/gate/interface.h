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
	const char* key;
	uint32_t keylen;
	uint64_t hash;

	shared_zone life;
	callback_get callback;
	void* user;

	void submit();
};


struct res_set {
	int error;

	const char* key;
	uint32_t keylen;
	uint64_t hash;

	const char* val;
	uint32_t vallen;
	uint64_t clocktime;
};

typedef void (*callback_set)(void* user, res_set& res, auto_zone z);

struct req_set {
	const char* key;
	uint32_t keylen;
	uint64_t hash;

	const char* val;
	uint32_t vallen;

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
	const char* key;
	uint32_t keylen;
	uint64_t hash;

	shared_zone life;
	callback_delete callback;
	void* user;

	void submit();
};


}  // namespace gate
}  // namespace kumo

#endif /* gate/interface.h */

