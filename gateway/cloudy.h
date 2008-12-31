#ifndef GATEWAH_CLOUDY_H__
#define GATEWAH_CLOUDY_H__

#include "gateway/gateway.h"

namespace kumo {


class Cloudy : public GatewayInterface {
public:
	Cloudy(int lsock);
	~Cloudy();

	static void accepted(Gateway* gw, int fd, int err);
	void listen(Gateway* gw);

private:
	class Connection;
	int m_lsock;

private:
	Cloudy();
	Cloudy(const Cloudy&);
};


}  // namespace kumo

#endif /* gateway/cloudy.h */


