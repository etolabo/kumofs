//
// mp::wavy::core
//
// Copyright (C) 2008-2009 FURUHASHI Sadayuki
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

#ifndef WAVY_CORE_H__
#define WAVY_CORE_H__

#include "mp/wavy/core.h"
#include "mp/pthread.h"
#include "wavy_edge.h"

namespace mp {
namespace wavy {


class core::impl {
public:
	impl();
	~impl();

public:
	void add_thread(size_t num);

	void end();
	bool is_end() const;

	void join();
	void detach();

	class connect_thread;

	class listen_handler;
	void listen(int lsock, listen_callback_t callback);

	class timer_thread;

public:
	inline void add_impl(int fd, handler* newh);
	inline void submit_impl(task_t& f);

public:
	void operator() ();

private:
	volatile size_t m_off;
	volatile size_t m_num;
	volatile bool m_pollable;

	edge::backlog m_backlog;

	typedef shared_ptr<handler> shared_handler;
	shared_handler* m_state;

	edge m_edge;

	pthread_mutex m_mutex;
	pthread_cond m_cond;

	volatile bool m_end_flag;

	typedef std::queue<task_t> task_queue_t;
	task_queue_t m_task_queue;

private:
	typedef std::vector<pthread_thread*> workers_t;
	workers_t m_workers;

private:
	impl(const impl&);
};


}  // namespace wavy
}  // namespace mp

#endif /* wavy_core.h */

