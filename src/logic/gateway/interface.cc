#include "gateway/framework.h"

namespace kumo {
namespace gateway {


void add_gate(gate* it)
{
	it->listen();
}


uint64_t stdhash(const char* key, size_t keylen)
{
	return HashSpace::hash(key, keylen);
}

// submit() is in framework.cc


}  // namespace gateway
}  // namespace kumo

