#ifndef GATEWAY_INTERFACE_H__
#define GATEWAY_INTERFACE_H__

#include "rpc/wavy.h"
#include "rpc/types.h"
#include <mp/utility.h>
#include <mp/memory.h>
#include <msgpack/zone.h>

namespace kumo {
namespace gateway {


typedef rpc::wavy wavy;
using rpc::shared_zone;


class gate {
public:
	gate() { }
	virtual ~gate() { }
	virtual void listen() = 0;
};


void add_gate(gate* it);



struct basic_response {
	uint64_t hash;
	const char* key;
	uint32_t keylen;
	shared_zone life;
	int error;
};

struct basic_request {
	uint64_t hash;
	const char* key;
	uint32_t keylen;
	shared_zone life;
};

struct get_response : basic_response {
	char* val;
	uint32_t vallen;
	uint64_t clocktime;
};

struct set_response : basic_response {
	const char* val;
	uint32_t vallen;
	uint64_t clocktime;
};

struct delete_response : basic_response {
	bool deleted;
};

struct get_request : basic_request {
	void (*callback)(void* user, get_response& res);
	void* user;
};

struct set_request : basic_request {
	void (*callback)(void* user, set_response& res);
	void* user;
	const char* val;
	uint32_t vallen;
};

struct delete_request : basic_request {
	void (*callback)(void* user, delete_response& res);
	void* user;
};


uint64_t stdhash(const char* key, size_t keylen);

void submit(get_request& req);
void submit(set_request& req);
void submit(delete_request& req);


}  // namespace gateway
}  // namespace kumo

#endif /* gateway/interface.h */

