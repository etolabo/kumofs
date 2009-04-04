#ifndef KUMO_GATE_MEMCACHE_TEXT_H__
#define KUMO_GATE_MEMCACHE_TEXT_H__

#include "gate/interface.h"

namespace kumo {


class MemcacheText : public gate::gate {
public:
	MemcacheText(int lsock);
	~MemcacheText();

	void run();

private:
	int m_lsock;

private:
	MemcacheText();
	MemcacheText(const MemcacheText&);
};


}  // namespace kumo

#endif /* gate/memcache_text.h */

