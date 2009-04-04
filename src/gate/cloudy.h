#ifndef KUMO_GATE_CLOUDY_H__
#define KUMO_GATE_CLOUDY_H__

#include "gate/interface.h"

namespace kumo {


class Cloudy : public gate::gate {
public:
	Cloudy(int lsock);
	~Cloudy();

	void run();

private:
	int m_lsock;

private:
	Cloudy();
	Cloudy(const Cloudy&);
};


}  // namespace kumo

#endif /* gate/cloudy.h */

