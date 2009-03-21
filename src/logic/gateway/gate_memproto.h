#ifndef GATEWAY_GATE_MEMPROTO_H__
#define GATEWAY_GATE_MEMPROTO_H__

#include "gateway/interface.h"

namespace kumo {


class Memproto : public gateway::gate {
public:
	Memproto(int lsock);
	~Memproto();

	static void accepted(int fd, int err);
	void listen();

private:
	int m_lsock;

private:
	Memproto();
	Memproto(const Memproto&);
};


}  // namespace kumo

#endif /* gateway/gate_memproto.h */

