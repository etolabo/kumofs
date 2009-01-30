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

#ifndef MP_PTHREAD_IMPL_H__
#define MP_PTHREAD_IMPL_H__

#include <iostream>
#include <typeinfo>
#ifndef MP_NO_CXX_ABI_H
#include <cxxabi.h>
#endif

namespace mp {


template <typename IMPL>
pthread_thread::pthread_thread(IMPL* pimpl) :
	m_thread_func(&pthread_thread::trampoline<IMPL>),
	m_pimpl(reinterpret_cast<void*>(pimpl))
{ }

inline pthread_thread::~pthread_thread()
{ }

inline void pthread_thread::run()
{
	int err = pthread_create(&m_thread, NULL,
			m_thread_func, m_pimpl);
	if(err) { throw pthread_error(err, "failed to create thread"); }
}

inline void pthread_thread::detach()
{
	int err = pthread_detach(m_thread);
	if(err) { throw pthread_error(err, "failed to detach thread"); }
}

inline void* pthread_thread::join()
{
	void* ret;
	int err = pthread_join(m_thread, &ret);
	if(err) { throw pthread_error(err, "failed to join thread"); }
	return ret;
}

inline void pthread_thread::cancel()
{
	pthread_cancel(m_thread);
}

inline bool pthread_thread::operator== (const pthread_thread& other) const
{
	return pthread_equal(m_thread, other.m_thread);
}

inline bool pthread_thread::operator!= (const pthread_thread& other) const
{
	return !(*this == other);
}

template <typename IMPL>
void* pthread_thread::trampoline(void* obj)
try {
	reinterpret_cast<IMPL*>(obj)->operator()();
	return NULL;  // FIXME

} catch (std::exception& e) {
	try {
#ifndef MP_NO_CXX_ABI_H
		int status;
		std::cerr
			<< "thread terminated with throwing an instance of '"
			<< abi::__cxa_demangle(typeid(e).name(), 0, 0, &status)
			<< "'\n"
			<< "  what():  " << e.what() << std::endl;
#else
		std::cerr
			<< "thread terminated with throwing an instance of '"
			<< typeid(e).name()
			<< "'\n"
			<< "  what():  " << e.what() << std::endl;
#endif
	} catch (...) {}
	throw;

} catch (...) {
	try {
		std::cerr << "thread terminated with throwing an unknown object" << std::endl;
	} catch (...) {}
	throw;
}


inline void pthread_thread::exit(void* retval)
{
	pthread_exit(retval);
}


template <typename IMPL>
pthread_thread_impl<IMPL>::pthread_thread_impl() :
	pthread_thread(this) { }

template <typename IMPL>
pthread_thread_impl<IMPL>::~pthread_thread_impl() { }


inline pthread_mutex::pthread_mutex(const pthread_mutexattr_t *attr)
{
	pthread_mutex_init(&m_mutex, attr);
}

inline pthread_mutex::pthread_mutex(int kind)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, kind);
	pthread_mutex_init(&m_mutex, &attr);
}

inline pthread_mutex::~pthread_mutex()
{
	pthread_mutex_destroy(&m_mutex);
}

inline void pthread_mutex::pthread_mutex::lock()
{
	int err = pthread_mutex_lock(&m_mutex);
	if(err != 0) { throw pthread_error(-err, "failed to lock pthread mutex"); }
}

inline void pthread_mutex::pthread_mutex::unlock()
{
	int err = pthread_mutex_unlock(&m_mutex);
	if(err != 0) { throw pthread_error(-err, "failed to unlock pthread mutex"); }
}

inline bool pthread_mutex::trylock()
{
	int err = pthread_mutex_trylock(&m_mutex);
	if(err != 0) {
		if(err == EBUSY) { return false; }
		throw pthread_error(-err, "failed to trylock pthread mutex");
	}
	return true;
}


inline pthread_rwlock::pthread_rwlock(const pthread_rwlockattr_t *attr)
{
	pthread_rwlock_init(&m_mutex, attr);
}

//inline pthread_rwlock::pthread_rwlock(int kind)
//{
//	pthread_rwlockattr_t attr;
//	pthread_rwlockattr_init(&attr);
//	pthread_rwlockattr_settype(&attr, kind);
//	pthread_rwlock_init(&m_mutex, &attr);
//}

inline pthread_rwlock::~pthread_rwlock()
{
	pthread_rwlock_destroy(&m_mutex);
}

inline void pthread_rwlock::pthread_rwlock::rdlock()
{
	int err = pthread_rwlock_rdlock(&m_mutex);
	if(err != 0) { throw pthread_error(-err, "failed to read lock pthread rwlock"); }
}

inline bool pthread_rwlock::tryrdlock()
{
	int err = pthread_rwlock_tryrdlock(&m_mutex);
	if(err != 0) {
		if(err == EBUSY) { return false; }
		throw pthread_error(-err, "failed to read trylock pthread rwlock");
	}
	return true;
}

inline void pthread_rwlock::pthread_rwlock::wrlock()
{
	int err = pthread_rwlock_wrlock(&m_mutex);
	if(err != 0) { throw pthread_error(-err, "failed to write lock pthread rwlock"); }
}

inline bool pthread_rwlock::trywrlock()
{
	int err = pthread_rwlock_trywrlock(&m_mutex);
	if(err != 0) {
		if(err == EBUSY) { return false; }
		throw pthread_error(-err, "failed to write trylock pthread rwlock");
	}
	return true;
}

inline void pthread_rwlock::pthread_rwlock::unlock()
{
	int err = pthread_rwlock_unlock(&m_mutex);
	if(err != 0) { throw pthread_error(-err, "failed to unlock pthread rwlock"); }
}


inline pthread_scoped_lock::pthread_scoped_lock() :
	m_mutex(NULL) { }

inline pthread_scoped_lock::pthread_scoped_lock(pthread_mutex& mutex) :
	m_mutex(NULL)
{
	mutex.lock();
	m_mutex = &mutex;
}

inline pthread_scoped_lock::~pthread_scoped_lock()
{
	if(m_mutex) {
		m_mutex->unlock();
	}
}

inline void pthread_scoped_lock::unlock()
{
	if(m_mutex) {
		m_mutex->unlock();
		m_mutex = NULL;
	}
}

inline void pthread_scoped_lock::relock(pthread_mutex& mutex)
{
	unlock();
	mutex.lock();
	m_mutex = &mutex;
}


inline pthread_scoped_rdlock::pthread_scoped_rdlock() :
	m_mutex(NULL) { }

inline pthread_scoped_rdlock::pthread_scoped_rdlock(pthread_rwlock& mutex) :
	m_mutex(NULL)
{
	mutex.rdlock();
	m_mutex = &mutex;
}

inline pthread_scoped_rdlock::~pthread_scoped_rdlock()
{
	if(m_mutex) {
		m_mutex->unlock();
	}
}

inline void pthread_scoped_rdlock::unlock()
{
	if(m_mutex) {
		m_mutex->unlock();
		m_mutex = NULL;
	}
}

inline void pthread_scoped_rdlock::relock(pthread_rwlock& mutex)
{
	unlock();
	mutex.rdlock();
	m_mutex = &mutex;
}


inline pthread_scoped_wrlock::pthread_scoped_wrlock() :
	m_mutex(NULL) { }

inline pthread_scoped_wrlock::pthread_scoped_wrlock(pthread_rwlock& mutex) :
	m_mutex(NULL)
{
	mutex.wrlock();
	m_mutex = &mutex;
}

inline pthread_scoped_wrlock::~pthread_scoped_wrlock()
{
	if(m_mutex) {
		m_mutex->unlock();
	}
}

inline void pthread_scoped_wrlock::unlock()
{
	if(m_mutex) {
		m_mutex->unlock();
		m_mutex = NULL;
	}
}

inline void pthread_scoped_wrlock::relock(pthread_rwlock& mutex)
{
	unlock();
	mutex.wrlock();
	m_mutex = &mutex;
}


inline pthread_cond::pthread_cond(const pthread_condattr_t *attr)
{
	pthread_cond_init(&m_cond, attr);
}

inline pthread_cond::~pthread_cond()
{
	pthread_cond_destroy(&m_cond);
}

inline void pthread_cond::signal()
{
	int err = pthread_cond_signal(&m_cond);
	if(err != 0) { throw pthread_error(-err, "failed to signal pthread cond"); }
}

inline void pthread_cond::broadcast()
{
	int err = pthread_cond_broadcast(&m_cond);
	if(err != 0) { throw pthread_error(-err, "failed to broadcast pthread cond"); }
}

inline void pthread_cond::wait(pthread_mutex& mutex)
{
	int err = pthread_cond_wait(&m_cond, mutex.get());
	if(err != 0) { throw pthread_error(-err, "failed to wait pthread cond"); }
}

inline bool pthread_cond::timedwait(pthread_mutex& mutex, const struct timespec *abstime)
{
	int err = pthread_cond_timedwait(&m_cond, mutex.get(), abstime);
	if(err != 0) {
		if(err == ETIMEDOUT) { return false; }
		throw pthread_error(-err, "failed to timedwait pthread cond");
	}
	return true;
}


inline pthread_signal::scoped_sigprocmask::scoped_sigprocmask(const sigset_t& ss) :
	m_ss(ss)
{
	if( sigprocmask(SIG_BLOCK, &m_ss, NULL) < 0 ) {
		throw pthread_error(errno, "failed to set sigprocmask");
	}
}

inline pthread_signal::scoped_sigprocmask::~scoped_sigprocmask()
{
	sigprocmask(SIG_UNBLOCK, &m_ss, NULL);
}

inline pthread_signal::pthread_signal(const sigset_t& ss, void (*handler)(void*, int), void* data) :
	m_sigmask(ss), m_handler(handler), m_data(data), m_thread(this)
{
	m_thread.run();
}

inline pthread_signal::~pthread_signal() {}

inline void pthread_signal::operator() ()
{
	int signo;
	while(true) {
		if(sigwait(m_sigmask.get(), &signo) != 0) { return; }
		(*m_handler)(m_data, signo);
	}
}


}  // namespace mp

#endif /* mp/pthread.h */

