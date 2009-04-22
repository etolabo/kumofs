//
// mp::wavy::output
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

#include "mp/wavy/output.h"
#include "mp/pthread.h"
#include "wavy_edge.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>
#include <algorithm>

#ifndef MP_WAVY_WRITEV_LIMIT
#define MP_WAVY_WRITEV_LIMIT 1024
#endif

//#ifndef MP_WAVY_WRITE_QUEUE_LIMIT
//#define MP_WAVY_WRITE_QUEUE_LIMIT 32
//#endif

namespace mp {
namespace wavy {


class output::impl {
public:
	impl();
	~impl();

	void add_thread(size_t num);

	void end();

	void join();
	void detach();

public:
	void writev(int fd, const iovec* bufvec, const request* reqvec, size_t veclen);

private:
	class context {
	public:
		context();
		~context();

	public:
		pthread_mutex& mutex();
#ifdef MP_WAVY_WRITE_QUEUE_LIMIT
		void wait_cond();
#endif

		bool push(const iovec* bufvec,
				const request* reqvec, size_t veclen);
		bool empty() const;
		size_t size() const;

		iovec* vec();
		size_t veclen() const;

		bool skip_zero();

		bool consumed(size_t num);
		void clear();

	private:
		typedef std::vector<iovec  > bufvec_t;
		typedef std::vector<request> reqvec_t;
		bufvec_t m_bufvec;
		reqvec_t m_reqvec;
		pthread_mutex m_mutex;
#ifdef MP_WAVY_WRITE_QUEUE_LIMIT
		pthread_cond m_cond;
		volatile bool m_wait;
#endif

	private:
		context(const context&);
	};

	context* m_fdctx;

	class worker : public pthread_thread {
	public:
		worker(context* fdctx, volatile bool& end_flag);
		~worker();

	public:
		bool try_write_initial(int fd);
		void watch(int fd);

	public:
		void operator() ();

	private:
		void initial_remove(int fd);

		void try_write(int fd);
		void success_remove(int fd);
		void failed_remove(int fd);

	private:
		context* m_fdctx;
		volatile bool& m_end_flag;
		edge::backlog m_backlog;
		edge m_edge;

	private:
		worker();
		worker(const worker&);
	};

	volatile bool m_end_flag;

private:
	worker* worker_for(int fd);

	typedef std::vector<worker*> workers_t;
	workers_t m_workers;

private:
	impl(const impl&);
};


output::output() : m_impl(new impl()) { }

output::impl::impl() :
	m_end_flag(false)
{
	struct rlimit rbuf;
	if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
		throw system_error(errno, "getrlimit() failed");
	}
	m_fdctx = new context[rbuf.rlim_cur];
}


output::~output() { }

output::impl::~impl()
{
	end();
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		delete *it;
	}
	delete[] m_fdctx;
}

void output::end() { m_impl->end(); }
void output::impl::end()
{
	m_end_flag = true;
}

void output::join() { m_impl->join(); }
void output::impl::join()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		(*it)->join();
	}
}

void output::detach() { m_impl->detach(); }
void output::impl::detach()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		(*it)->detach();
	}
}

void output::add_thread(size_t num) { m_impl->add_thread(num); }
void output::impl::add_thread(size_t num)
{
	for(size_t i=0; i < num; ++i) {
		m_workers.push_back(NULL);
		try {
			m_workers.back() = new worker(m_fdctx, m_end_flag);
		} catch (...) {
			m_workers.pop_back();
			throw;
		}
		m_workers.back()->run();
	}
}

output::impl::worker* output::impl::worker_for(int fd)
{
	return m_workers[fd % m_workers.size()];
}


output::impl::context::context() /*: m_wait(false)*/ { }

output::impl::context::~context() { clear(); }

inline bool output::impl::context::skip_zero()
{
	size_t offset = 0;
	for(; offset < veclen() && vec()[offset].iov_len == 0; ++offset) { }
	return consumed(offset);
}

inline bool output::impl::context::consumed(size_t num)
{
	if(num == 0) { return m_bufvec.empty(); }
	for(size_t i=0; i < num; ++i) {
		if(m_reqvec[i].finalize) {
			(*m_reqvec[i].finalize)(m_reqvec[i].user);
		}
	}
	m_bufvec.erase(m_bufvec.begin(), m_bufvec.begin()+num);
	m_reqvec.erase(m_reqvec.begin(), m_reqvec.begin()+num);
#ifdef MP_WAVY_WRITE_QUEUE_LIMIT
	if(size() > MP_WAVY_WRITE_QUEUE_LIMIT) {
		return false;
	}
	if(m_wait) {
		m_cond.broadcast();
		m_wait = false;
	}
	return m_bufvec.empty();
#else
	return m_bufvec.empty();
#endif
}

inline void output::impl::context::clear()
{
	consumed(m_bufvec.size());
}

bool output::impl::context::push(const iovec* bufvec,
		const request* reqvec, size_t veclen)
{
	bool watch_needed = m_bufvec.empty();
	m_bufvec.insert(m_bufvec.end(), bufvec, bufvec+veclen);
	m_reqvec.insert(m_reqvec.end(), reqvec, reqvec+veclen);
	return watch_needed;
}

pthread_mutex& output::impl::context::mutex()
{
	return m_mutex;
}

#ifdef MP_WAVY_WRITE_QUEUE_LIMIT
void output::impl::context::wait_cond()
{
	m_wait = true;
	m_cond.wait(m_mutex);
}
#endif

iovec* output::impl::context::vec()
{
	return &m_bufvec.front();
}

size_t output::impl::context::veclen() const
{
	return m_bufvec.size();
}

bool output::impl::context::empty() const
{
	return m_bufvec.empty();
}

size_t output::impl::context::size() const
{
	return m_bufvec.size();
}


output::impl::worker::worker(context* fdctx, volatile bool& end_flag) :
	pthread_thread(this),
	m_fdctx(fdctx),
	m_end_flag(end_flag)
{ }

output::impl::worker::~worker() { }

void output::impl::worker::watch(int fd)
{
	if(m_edge.add_notify(fd, EVEDGE_WRITE) < 0) {
		// FIXME
		failed_remove(fd);
	}
}

void output::impl::worker::operator() ()
{
	while(!m_end_flag) {
		int num = m_edge.wait(&m_backlog, 1000);
		if(num < 0) {
			if(errno == EINTR || errno == EAGAIN) {
				continue;
			} else {
				throw system_error(errno, "wavy output event failed");
			}
		} else if(num == 0) {
			continue;
		}
		for(int i=0; i < num; ++i) {
			int fd = m_backlog[i];
			try_write(fd);
		}
	}
}

void output::impl::worker::try_write(int fd)
{
	context& ctx(m_fdctx[fd]);
	pthread_scoped_lock lk(ctx.mutex());

	if(ctx.skip_zero()) {
		success_remove(fd);
		return;
	}

	ssize_t wl = ::writev(fd, ctx.vec(),
			std::min(ctx.veclen(), (size_t)MP_WAVY_WRITEV_LIMIT));
	if(wl < 0) {
		if(errno == EAGAIN || errno == EINTR) {
			return;
		} else {
			failed_remove(fd);
			return;
		}
	} else if(wl == 0) {
		failed_remove(fd);
		return;
	}

	size_t i;
	for(i=0; i < ctx.veclen(); ++i) {
		if(static_cast<size_t>(wl) >= ctx.vec()[i].iov_len) {
			wl -= ctx.vec()[i].iov_len;
		} else {
			ctx.vec()[i].iov_base = (void*)(((char*)ctx.vec()[i].iov_base) + wl);
			ctx.vec()[i].iov_len -= wl;
			break;
		}
	}
	if(ctx.consumed(i)) {
		success_remove(fd);
		return;
	}
	if(m_edge.shot_reactivate(fd, EVEDGE_WRITE) < 0) {
		// FIXME
		//failed_remove(fd);
	}
}

bool output::impl::worker::try_write_initial(int fd)
{
	context& ctx(m_fdctx[fd]);

	if(ctx.skip_zero()) {
		return true;
	}

	ssize_t wl = ::writev(fd, ctx.vec(),
			std::min(ctx.veclen(), (size_t)MP_WAVY_WRITEV_LIMIT));
	if(wl < 0) {
		if(errno == EAGAIN || errno == EINTR) {
			return false;
		} else {
			initial_remove(fd);
			return true;
		}
	} else if(wl == 0) {
		initial_remove(fd);
		return true;
	}

	size_t i;
	for(i=0; i < ctx.veclen(); ++i) {
		if(static_cast<size_t>(wl) >= ctx.vec()[i].iov_len) {
			wl -= ctx.vec()[i].iov_len;
		} else {
			ctx.vec()[i].iov_base = (void*)(((char*)ctx.vec()[i].iov_base) + wl);
			ctx.vec()[i].iov_len -= wl;
			break;
		}
	}
	return ctx.consumed(i);
}

inline void output::impl::worker::initial_remove(int fd)
{
	m_fdctx[fd].clear();
	::shutdown(fd, SHUT_RD);  // FIXME shutdown() only break socket
}

inline void output::impl::worker::failed_remove(int fd)
{
	m_fdctx[fd].clear();
	m_edge.shot_remove(fd, EVEDGE_WRITE);
	// break fd positively
	// input side will catch the exception.
	::shutdown(fd, SHUT_RD);  // FIXME shutdown() only break socket
}

inline void output::impl::worker::success_remove(int fd)
{
	m_edge.shot_remove(fd, EVEDGE_WRITE);  // ignore error
}


void output::write(int fd, const char* buf, size_t buflen)
{
	struct iovec bufvec = {(void*)buf, buflen};
	request reqvec(NULL, NULL);
	m_impl->writev(fd, &bufvec, &reqvec, 1);
}

void output::writev(int fd, const iovec* vec, size_t veclen)
{
	request reqvec[veclen];
	memset(reqvec, 0, sizeof(request)*veclen);
	m_impl->writev(fd, vec, reqvec, veclen);
}

void output::write(int fd, const char* buf, size_t buflen, request req)
{
	struct iovec bufvec = {(void*)buf, buflen};
	m_impl->writev(fd, &bufvec, &req, 1);
}

void output::write(int fd, const char* buf, size_t buflen, finalize_t finalize, void* user)
{
	struct iovec bufvec = {(void*)buf, buflen};
	request req(finalize, user);
	m_impl->writev(fd, &bufvec, &req, 1);
}

void output::writev(int fd, const iovec* vec, size_t veclen, finalize_t finalize, void* user)
{
	request reqvec[veclen];
	memset(reqvec, 0, sizeof(request)*(veclen-1));
	reqvec[veclen-1] = request(finalize, user);
	m_impl->writev(fd, vec, reqvec, veclen);
}

void output::writev(int fd, const iovec* vec, size_t veclen, request req)
{
	request reqvec[veclen];
	memset(reqvec, 0, sizeof(request)*(veclen-1));
	reqvec[veclen-1] = req;
	m_impl->writev(fd, vec, reqvec, veclen);
}

void output::writev(int fd, const iovec* bufvec, const request* reqvec, size_t veclen)
{
	m_impl->writev(fd, bufvec, reqvec, veclen);
}

void output::impl::writev(int fd, const iovec* bufvec, const request* reqvec, size_t veclen)
{
	context& ctx(m_fdctx[fd]);
	pthread_scoped_lock lk(ctx.mutex());
	if(ctx.push(bufvec, reqvec, veclen)) {
#ifndef MP_WAVY_NO_TRY_WRITE_INITIAL
		if(!worker_for(fd)->try_write_initial(fd)) {
			worker_for(fd)->watch(fd);
		}
#else
		worker_for(fd)->watch(fd);
#endif
	} else {
#ifdef MP_WAVY_WRITE_QUEUE_LIMIT
		// FIXME sender or receiver must not wait to flush to avoid deadlock.
		while(ctx.size() > MP_WAVY_WRITE_QUEUE_LIMIT) {
			ctx.wait_cond();
		}
#endif
	}
}


}  // namespace wavy
}  // namespace mp

