#ifndef GATEWAY_SCOPE_STORE_H__
#define GATEWAY_SCOPE_STORE_H__

#include "gateway/interface.h"
#include "server/proto_store.h"

namespace kumo {
namespace gateway {


class scope_store {
public:
	scope_store();
	~scope_store();

public:
	void Get(void (*callback)(void*, get_response&), void* user,
			shared_zone life,
			const char* key, uint32_t keylen, uint64_t hash);

	void Set(void (*callback)(void*, set_response&), void* user,
			shared_zone life,
			const char* key, uint32_t keylen, uint64_t hash,
			const char* val, uint32_t vallen);

	void Delete(void (*callback)(void*, delete_response&), void* user,
			shared_zone life,
			const char* key, uint32_t keylen, uint64_t hash);

private:
	RPC_REPLY_DECL(Get, from, res, err, life,
			rpc::retry<server::proto_store::Get>* retry,
			void (*callback)(void*, get_response&), void* user);

	RPC_REPLY_DECL(Set, from, res, err, life,
			rpc::retry<server::proto_store::Set>* retry,
			void (*callback)(void*, set_response&), void* user);

	RPC_REPLY_DECL(Delete, from, res, err, life,
			rpc::retry<server::proto_store::Delete>* retry,
			void (*callback)(void*, delete_response&), void* user);
};


}  // namespace gateway
}  // namespace kumo

#endif /* gateway/scope_store.h */

