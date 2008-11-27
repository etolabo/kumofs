#ifndef GATEWAH_MEMPROTO_TEXT_H__
#define GATEWAH_MEMPROTO_TEXT_H__

#include "gateway/gateway.h"

namespace kumo {


class MemprotoText : public GatewayInterface {
public:
	MemprotoText(int lsock);
	~MemprotoText();

	static void accepted(void* data, int fd);
	void listen(Gateway* gw);

private:
	class Connection;
	class SharedResponder;
	int m_lsock;

private:
	MemprotoText();
	MemprotoText(const MemprotoText&);
};


}  // namespace kumo

#endif /* gateway/memcache_text.h */

