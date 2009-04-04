#include "gateway/framework.h"

namespace kumo {
namespace gate {


uint64_t stdhash(const char* key, size_t keylen)
{
	return HashSpace::hash(key, keylen);
}

void fatal_stop()
{
	gateway::net->signal_end();
}


void req_get::submit()
{
	gateway::net->mod_store.Get(
			callback, user, life,
			key, keylen, hash);
}

void req_set::submit()
{
	gateway::net->mod_store.Set(
			callback, user, life,
			key, keylen, hash,
			val, vallen);
}

void req_delete::submit()
{
	gateway::net->mod_store.Delete(
			callback, user, life,
			key, keylen, hash);
}


}  // namespace gate
}  // namespace kumo

