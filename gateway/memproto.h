#ifndef GATEWAH_MEMPROTO_H__
#define GATEWAH_MEMPROTO_H__

#include "gateway/gateway.h"

namespace kumo {


class Memproto : public GatewayInterface {
public:
	Memproto(int lsock);
	~Memproto();

	static void accepted(Gateway* gw, int fd, int err);
	void listen(Gateway* gw);

private:
	class Connection;
	int m_lsock;

private:
	Memproto();
	Memproto(const Memproto&);
};


}  // namespace kumo

#endif /* gateway/memproto.h */

