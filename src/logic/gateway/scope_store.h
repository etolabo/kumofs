#ifndef GATEWAY_SCOPE_STORE_H__
#define GATEWAY_SCOPE_STORE_H__

#include "gate/interface.h"
#include "server/proto_store.h"

namespace kumo {
namespace gateway {


class scope_store {
public:
	scope_store();
	~scope_store();

public:
	void Get(gate::callback_get callback, void* user,
			shared_zone life,
			const char* key, uint32_t keylen, uint64_t hash);

	void Set(gate::callback_set callback, void* user,
			shared_zone life,
			const char* key, uint32_t keylen, uint64_t hash,
			const char* val, uint32_t vallen);

	void Delete(gate::callback_delete callback, void* user,
			shared_zone life,
			const char* key, uint32_t keylen, uint64_t hash);

private:
	RPC_REPLY_DECL(Get, from, res, err, z,
			rpc::retry<server::proto_store::Get>* retry,
			gate::callback_get callback, void* user);

	RPC_REPLY_DECL(Set, from, res, err, z,
			rpc::retry<server::proto_store::Set>* retry,
			gate::callback_set callback, void* user);

	RPC_REPLY_DECL(Delete, from, res, err, z,
			rpc::retry<server::proto_store::Delete>* retry,
			gate::callback_delete callback, void* user);
};


}  // namespace gateway
}  // namespace kumo

#endif /* gateway/scope_store.h */

