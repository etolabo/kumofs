#ifndef GATEWAH_MEMPROTO_H__
#define GATEWAH_MEMPROTO_H__

#include "gateway/gateway.h"

namespace kumo {


class Memproto : public GatewayInterface {
public:
	Memproto(Gateway& cli);
	~Memproto();

	void add_connection(int fd);

private:
	class Connection;
	class SharedResponder;
	Gateway& m_gw;

private:
	Memproto();
	Memproto(const Memproto&);
};


}  // namespace kumo

#endif /* gateway/memproto.h */

