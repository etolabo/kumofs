#ifndef LOGIC_WAVY_SERVER_H__
#define LOGIC_WAVY_SERVER_H__

#include "rpc/wavy.h"
#include "log/mlogger.h"
#include "log/logpacker.h"
#include <mp/pthread.h>

namespace kumo {


using rpc::wavy;


class wavy_server {
public:
	wavy_server();
	~wavy_server();

protected:
	void init_wavy(unsigned short rthreads, unsigned short wthreads);

	virtual void end_preprocess() { }

public:
	virtual void run();
	virtual void join();

	void signal_handler(int signo);
	void signal_end();
	void signal_hup();

private:
	unsigned short m_core_threads;
	unsigned short m_output_threads;
	std::auto_ptr<mp::pthread_signal> s_pth;
};


}  // namespace kumo

#endif /* logic/wavy_server.h */

