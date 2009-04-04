#ifndef KUMO_GATE_MEMCACHE_BINARY_H__
#define KUMO_GATE_MEMCACHE_BINARY_H__

#include "gate/interface.h"

namespace kumo {


class MemcacheBinary : public gate::gate {
public:
	MemcacheBinary(int lsock);
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

