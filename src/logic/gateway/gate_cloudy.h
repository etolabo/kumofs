#ifndef GATEWAY_GATE_CLOUDY_H__
#define GATEWAY_GATE_CLOUDY_H__

#include "gateway/interface.h"

namespace kumo {


class Cloudy : public gateway::gate {
public:
	Cloudy(int lsock);
	~Cloudy();

	static void accepted(int fd, int err);
	void listen();

private:
	class Connection;
	int m_lsock;

private:
	Cloudy();
	Cloudy(const Cloudy&);
};


}  // namespace kumo

#endif /* gateway/gate_cloudy.h */

