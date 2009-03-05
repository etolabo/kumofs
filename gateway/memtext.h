#ifndef GATEWAH_MEMTEXT_H__
#define GATEWAH_MEMTEXT_H__

#include "gateway/gateway.h"

namespace kumo {


class Memtext : public GatewayInterface {
public:
	Memtext(int lsock);
	~Memtext();

	static void accepted(Gateway* gw, int fd, int err);
	void listen(Gateway* gw);

private:
	class Connection;
	int m_lsock;

private:
	Memtext();
	Memtext(const Memtext&);
};


}  // namespace kumo

#endif /* gateway/memcache_text.h */

