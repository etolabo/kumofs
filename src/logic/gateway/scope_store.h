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
	RPC_REPLY_DECL(Get_1, from, res, err, life,
			rpc::retry<server::proto_store::Get_1>* retry,
			void (*callback)(void*, get_response&), void* user);

	RPC_REPLY_DECL(Set_1, from, res, err, life,
			rpc::retry<server::proto_store::Set_1>* retry,
			void (*callback)(void*, set_response&), void* user);

	RPC_REPLY_DECL(Delete_1, from, res, err, life,
			rpc::retry<server::proto_store::Delete_1>* retry,
			void (*callback)(void*, delete_response&), void* user);

	enum hash_space_type {
		HS_WRITE,
		HS_READ,
	};
	template <hash_space_type Hs>
	shared_session server_for(uint64_t h, unsigned int offset = 0);

	void incr_error_count();
	unsigned short m_error_count;

	template <typename Parameter>
	void retry_after(unsigned int steps, rpc::retry<Parameter>* retry,
			uint64_t for_hash, shared_zone life);

	template <typename Parameter>
	struct retry_after_callback;
};


}  // namespace gateway
}  // namespace kumo

#endif /* gateway/scope_store.h */

