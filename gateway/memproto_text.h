#ifndef GATEWAH_MEMPROTO_TEXT_H__
#define GATEWAH_MEMPROTO_TEXT_H__

#include "gateway/gateway.h"

namespace kumo {


class MemprotoText : public GatewayInterface {
public:
	MemprotoText(Gateway* gw);
	~MemprotoText();

	void add(int fd);

private:
	class Connection;
	class SharedResponder;
	Gateway* m_gw;

private:
	MemprotoText();
	MemprotoText(const MemprotoText&);
};


}  // namespace kumo

#endif /* gateway/memcache_text.h */

