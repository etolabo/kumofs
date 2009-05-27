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

#include "wavy_core.h"
#include "mp/object_callback.h"
#include "mp/utility.h"
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>

#ifndef MP_WAVY_TASK_QUEUE_LIMIT
#define MP_WAVY_TASK_QUEUE_LIMIT 16
#endif

namespace mp {
namespace wavy {


core::core() : m_impl(new impl()) { }

core::impl::impl() :
	m_off(0),
	m_num(0),
	m_pollable(true),
	m_end_flag(false)
{
	struct rlimit rbuf;
	if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
		throw system_error(errno, "getrlimit() failed");
	}
	m_state = new shared_handler[rbuf.rlim_cur];
}


core::~core() { }

core::impl::~impl()
{
	end();
	{
		pthread_scoped_lock lk(m_mutex);
		m_cond.broadcast();
	}
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		delete *it;
	}
	delete[] m_state;
}


void core::end() { m_impl->end(); }
void core::impl::end()
{
	m_end_flag = true;
	{
		pthread_scoped_lock lk(m_mutex);
		m_cond.broadcast();
	}
}

bool core::is_end() const { return m_impl->is_end(); }
bool core::impl::is_end() const
{
	return m_end_flag;
}

void core::join() { m_impl->join(); }
void core::impl::join()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		(*it)->join();
	}
}

void core::detach() { m_impl->detach(); }
void core::impl::detach()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		(*it)->detach();
	}
}

void core::add_thread(size_t num) { m_impl->add_thread(num); }
void core::impl::add_thread(size_t num)
{
	for(size_t i=0; i < num; ++i) {
		m_workers.push_back(NULL);
		try {
			m_workers.back() = new pthread_thread(this);
		} catch (...) {
			m_workers.pop_back();
			throw;
		}
		m_workers.back()->run();
	}
}


void core::impl::submit_impl(task_t& f)
{
	pthread_scoped_lock lk(m_mutex);
	m_task_queue.push(f);
	m_cond.signal();
}
void core::submit_impl(task_t f)
	{ m_impl->submit_impl(f); }


void core::impl::add_impl(int fd, handler* newh)
{
	try {
		mp::set_nonblock(fd);
	} catch (...) {
		delete newh;
		throw;
	}
	m_state[fd].reset(newh);
	newh->m_shared_self = &m_state[fd];
	m_edge.add_notify(fd, EVEDGE_READ);
}
void core::add_impl(int fd, handler* newh)
	{ m_impl->add_impl(fd, newh); }


void core::impl::operator() ()
{
	retry:
	while(true) {
		pthread_scoped_lock lk(m_mutex);

		while(m_task_queue.size() > MP_WAVY_TASK_QUEUE_LIMIT || !m_pollable) {
			if(m_end_flag) { return; }

			if(!m_task_queue.empty()) {
				task_t ev = m_task_queue.front();
				m_task_queue.pop();
				if(!m_task_queue.empty()) { m_cond.signal(); }
				lk.unlock();
				try {
					ev();
				} catch (...) { }
				goto retry;
			}

			m_cond.wait(m_mutex);
		}

		if(m_num == m_off) {
			m_pollable = false;
			lk.unlock();

		retry_poll:
			if(m_end_flag) { return; }

			int num = m_edge.wait(&m_backlog, 1000);
			if(num < 0) {
				if(errno == EINTR || errno == EAGAIN) {
					goto retry_poll;
				} else {
					throw system_error(errno, "wavy core event failed");
				}
			} else if(num == 0) {
				goto retry_poll;
			}

			lk.relock(m_mutex);
			m_off = 0;
			m_num = num;

			m_pollable = true;
			m_cond.signal();
		}

		int fd = m_backlog[m_off];
		++m_off;
		lk.unlock();

		try {
			m_state[fd]->read_event();
		} catch (...) {
			m_edge.shot_remove(fd, EVEDGE_READ);
			m_state[fd]->m_shared_self = NULL;
			m_state[fd].reset();
			goto retry;
		}

		m_edge.shot_reactivate(fd, EVEDGE_READ);
	}
}


}  // namespace wavy
}  // namespace mp

