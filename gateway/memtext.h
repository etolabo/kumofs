#ifndef GATEWAH_MEMTEXT_H__
#define GATEWAH_MEMTEXT_H__

#include "gateway/gateway.h"

namespace kumo {


class MemprotoText : public GatewayInterface {
public:
	MemprotoText(int lsock);
	~MemprotoText();

	static void accepted(Gateway* gw, int fd, int err);
	void listen(Gateway* gw);

private:
	class Connection;
	int m_lsock;

private:
	MemprotoText();
	MemprotoText(const MemprotoText&);
};


}  // namespace kumo

#endif /* gateway/memcache_text.h */

