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
	gateway::net->mod_store.Get(*this);
}

void req_set::submit()
{
	gateway::net->mod_store.Set(*this);
}

void req_delete::submit()
{
	gateway::net->mod_store.Delete(*this);
}


}  // namespace gate
}  // namespace kumo

