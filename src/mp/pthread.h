//
// mpio pthread
//
// Copyright (C) 2008 FURUHASHI Sadayuki
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

#ifndef MP_PTHREAD_H__
#define MP_PTHREAD_H__

#include "mp/exception.h"
#include <pthread.h>
#include <signal.h>

namespace mp {


struct pthread_error : system_error {
	pthread_error(int errno_, const std::string& msg) :
		system_error(errno_, msg) {}
};


struct pthread_thread {
	template <typename IMPL>
	pthread_thread(IMPL* pimpl);

	virtual ~pthread_thread();

	void run();
	void detach();
	void* join();
	void cancel();

	bool operator== (const pthread_thread& other) const;
	bool operator!= (const pthread_thread& other) const;

	static void exit(void* retval = NULL);

private:
	pthread_t m_thread;
	void* (*m_thread_func)(void*);
	void* m_pimpl;

	template <typename IMPL>
	static void* trampoline(void* obj);
};


template <typename IMPL>
struct pthread_thread_impl : public pthread_thread {
	pthread_thread_impl();
	virtual ~pthread_thread_impl();
};


class pthread_mutex {
public:
	pthread_mutex(const pthread_mutexattr_t *attr = NULL);
	pthread_mutex(int kind);
	~pthread_mutex();
public:
	void lock();
	bool trylock();
	void unlock();
public:
	pthread_mutex_t* get() { return &m_mutex; }
private:
	pthread_mutex_t m_mutex;
private:
	pthread_mutex(const pthread_mutex&);
};


class pthread_rwlock {
public:
	pthread_rwlock(const pthread_rwlockattr_t *attr = NULL);
	//pthread_rwlock(int kind);
	~pthread_rwlock();
public:
	void rdlock();
	bool tryrdlock();
	void wrlock();
	bool trywrlock();
	void unlock();
public:
	pthread_rwlock_t* get() { return &m_mutex; }
private:
	pthread_rwlock_t m_mutex;
private:
	pthread_rwlock(const pthread_rwlock&);
};


class pthread_scoped_lock {
public:
	pthread_scoped_lock();
	pthread_scoped_lock(pthread_mutex& mutex);
	~pthread_scoped_lock();
public:
	void unlock();
	void relock(pthread_mutex& mutex);
private:
	pthread_mutex* m_mutex;
private:
	pthread_scoped_lock(const pthread_scoped_lock&);
};


class pthread_scoped_rdlock {
public:
	pthread_scoped_rdlock();
	pthread_scoped_rdlock(pthread_rwlock& mutex);
	~pthread_scoped_rdlock();
public:
	void unlock();
	void relock(pthread_rwlock& mutex);
private:
	pthread_rwlock* m_mutex;
private:
	pthread_scoped_rdlock(const pthread_scoped_rdlock&);
};

class pthread_scoped_wrlock {
public:
	pthread_scoped_wrlock();
	pthread_scoped_wrlock(pthread_rwlock& mutex);
	~pthread_scoped_wrlock();
public:
	void unlock();
	void relock(pthread_rwlock& mutex);
private:
	pthread_rwlock* m_mutex;
private:
	pthread_scoped_wrlock(const pthread_scoped_wrlock&);
};


class pthread_cond {
public:
	pthread_cond(const pthread_condattr_t *attr = NULL);
	~pthread_cond();
public:
	void signal();
	void broadcast();
	void wait(pthread_mutex& mutex);
	bool timedwait(pthread_mutex& mutex, const struct timespec *abstime);
public:
	pthread_cond_t* get() { return &m_cond; }
private:
	pthread_cond_t m_cond;
private:
	pthread_cond(const pthread_cond&);
};


class pthread_signal {
public:
	pthread_signal(const sigset_t& ss, void (*handler)(void*, int), void* data);
	~pthread_signal();
public:
	void operator() ();
private:
	struct scoped_sigprocmask {
		scoped_sigprocmask(const sigset_t& ss);
		~scoped_sigprocmask();
		const sigset_t* get() const { return &m_ss; }
	private:
		sigset_t m_ss;
		scoped_sigprocmask();
		scoped_sigprocmask(const scoped_sigprocmask&);
	};
	scoped_sigprocmask m_sigmask;
	void (*m_handler)(void*, int);
	void* m_data;
	mp::pthread_thread m_thread;
};


}  // namespace mp

#include "mp/pthread_impl.h"

#endif /* mp/pthread.h */

