#ifndef GATEWAH_MEMPROTO_H__
#define GATEWAH_MEMPROTO_H__

#include "gateway/gateway.h"

namespace kumo {


class Memproto : public GatewayInterface {
public:
	Memproto(Gateway& cli);
	~Memproto();

	static void accepted(void* data, int fd);
	void listen(Gateway* gw);

private:
	class Connection;
	class SharedResponder;
	int m_lsock;

private:
	Memproto();
	Memproto(const Memproto&);
};


}  // namespace kumo

#endif /* gateway/memproto.h */

