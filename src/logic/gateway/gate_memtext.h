#ifndef GATEWAY_GATE_MEMTEXT_H__
#define GATEWAY_GATE_MEMTEXT_H__

#include "gateway/interface.h"

namespace kumo {


class Memtext : public gateway::gate {
public:
	Memtext(int lsock);
	~Memtext();

	static void accepted(int fd, int err);
	void listen();

private:
	class Connection;
	int m_lsock;

private:
	Memtext();
	Memtext(const Memtext&);
};


}  // namespace kumo

#endif /* gateway/gate_memtext.h */

