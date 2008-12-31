#ifndef LOGIC_BASE_H__
#define LOGIC_BASE_H__

#include "kazuhiki/kazuhiki.h"
#include "log/mlogger_tty.h"
#include "log/mlogger_ostream.h"
#include "logic/global.h"
#include "rpc/wavy.h"
#include <mp/object_callback.h>
#include <mp/pthread.h>

namespace kumo {

using rpc::wavy;


class scoped_listen_tcp {
public:
	scoped_listen_tcp(struct sockaddr_in addr);
	~scoped_listen_tcp();

public:
	static int listen(const rpc::address& addr);

public:
	int sock() const
		{ return m_sock; }

	rpc::address addr() const
		{ return rpc::address(m_addr); }

private:
	rpc::address m_addr;
	int m_sock;

private:
	scoped_listen_tcp();
	scoped_listen_tcp(const scoped_listen_tcp&);
};


void do_daemonize(bool close_stdio, const char* pidfile);


struct rpc_server_args {
	rpc_server_args();
	~rpc_server_args();

	bool verbose;

	bool logfile_set;
	std::string logfile;

	bool pidfile_set;
	std::string pidfile;

	const char* prog;

	double keepalive_interval;  // sec
	unsigned long keepalive_interval_usec;  // convert

	double clock_interval;  // sec
	unsigned long clock_interval_usec;  // convert

	double connect_timeout_sec;
	unsigned int connect_timeout_msec;  // convert

	unsigned short connect_retry_limit;

	unsigned short wthreads;
	unsigned short rthreads;
	unsigned short cthreads;

public:
	virtual void set_basic_args();
	virtual void show_usage();

	void parse(int argc, char** argv);

protected:
	virtual void convert();
};


struct rpc_cluster_args : rpc_server_args {
	rpc_cluster_args();
	~rpc_cluster_args();

	virtual void set_basic_args();
	virtual void show_usage();

	struct sockaddr_in cluster_addr_in;
	rpc::address cluster_addr;  // convert
	int cluster_lsock;  // convert

protected:
	virtual void convert();
};


class wavy_server {
protected:
	template <typename Config>
	void init_wavy(Config& cfg)
	{
		// ignore SIGPIPE
		if( signal(SIGPIPE, SIG_IGN) == SIG_ERR ) {
			perror("signal");
			throw mp::system_error(errno, "signal");
		}
	
		// initialize signal handler before starting threads
		sigset_t ss;
		sigemptyset(&ss);
		sigaddset(&ss, SIGHUP);
		sigaddset(&ss, SIGINT);
		sigaddset(&ss, SIGTERM);

		s_pth.reset( new mp::pthread_signal(ss,
					get_signal_end(),
					reinterpret_cast<void*>(this)) );

		// initialize wavy
		m_core_threads = cfg.rthreads;
		m_output_threads = cfg.wthreads;
		wavy::initialize(0,0,2,2);  // FIXME
	}

	virtual void end_preprocess() { }

public:
	virtual void run()
	{
		wavy::add_output_thread(m_output_threads);
		wavy::add_core_thread(m_core_threads);
	}

	virtual void join()
	{
		wavy::join();
	}

public:
	void signal_end(int signo)
	{
		wavy::end();
		wavy::submit(finished);  // submit dummy function
		end_preprocess();
		LOG_INFO("end");
	}

private:
	static void finished() { }

	// avoid compile error
	typedef void (*sigend_callback)(void*, int);
	static sigend_callback get_signal_end()
	{
		sigend_callback f = &mp::object_callback<void (int)>::
			mem_fun<wavy_server, &wavy_server::signal_end>;
		return f;
	}

	unsigned short m_core_threads;
	unsigned short m_output_threads;
	std::auto_ptr<mp::pthread_signal> s_pth;
};


}  // namespace kumo

#endif /* logic/base.h */

