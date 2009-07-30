//
// kumofs
//
// Copyright (C) 2009 Etolabo Corp.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#ifndef LOGIC_WAVY_SERVER_H__
#define LOGIC_WAVY_SERVER_H__

#include "rpc/wavy.h"
#include "log/mlogger.h"
#include "log/logpacker.h"
#include <mp/pthread.h>
#include <mp/functional.h>
#include <list>

namespace kumo {


using rpc::wavy;


class wavy_server {
public:
	wavy_server();
	~wavy_server();

	void do_after(unsigned int steps, mp::function<void ()> func);

protected:
	// call this function before starting any threads
	void init_wavy(unsigned short rthreads, unsigned short wthreads);

	virtual void end_preprocess() { }

	void step_do_after();

public:
	virtual void join();

	void signal_handler(int signo);
	void signal_end();
	void signal_hup();

private:
	unsigned short m_core_threads;
	unsigned short m_output_threads;
	std::auto_ptr<mp::pthread_signal> s_pth;

	struct do_after_entry {
		do_after_entry(unsigned int steps, mp::function<void ()> f) :
			remain_steps(steps), func(f) { }
		unsigned int remain_steps;
		mp::function<void ()> func;
	};

	mp::pthread_mutex m_do_after_mutex;
	typedef std::list<do_after_entry> do_after_t;
	do_after_t m_do_after;
};


}  // namespace kumo

#endif /* logic/wavy_server.h */

