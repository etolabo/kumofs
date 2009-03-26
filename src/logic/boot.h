#ifndef LOGIC_BOOT_H__
#define LOGIC_BOOT_H__

#include "kazuhiki/kazuhiki.h"
#include "rpc/address.h"
#include "log/mlogger_tty.h"
#include "log/mlogger_ostream.h"
#include "log/logpacker.h"
#include "logic/global.h"

namespace kumo {


class scoped_listen_tcp {
public:
	scoped_listen_tcp(struct sockaddr_in addr);
	~scoped_listen_tcp();

public:
	static int listen(const rpc::address& addr);

public:
	int sock() const
	{
		return m_sock;
	}

	rpc::address addr() const
	{
		return rpc::address(m_addr);
	}

private:
	rpc::address m_addr;
	int m_sock;

private:
	scoped_listen_tcp();
	scoped_listen_tcp(const scoped_listen_tcp&);
};


void do_daemonize(bool close_stdio, const char* pidfile);

void init_mlogger(const std::string& logfile, bool use_tty, mlogger::level level);

struct rpc_args {
	rpc_args();
	~rpc_args();

	bool verbose;

	bool logfile_set;
	std::string logfile;

	bool logpack_path_set;
	std::string logpack_path;

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

public:
	virtual void set_basic_args();
	virtual void show_usage();

	void parse(int argc, char** argv);

protected:
	virtual void convert();
};


struct cluster_args : rpc_args {
	cluster_args();
	~cluster_args();

	virtual void set_basic_args();
	virtual void show_usage();

	struct sockaddr_in cluster_addr_in;
	rpc::address cluster_addr;  // convert
	int cluster_lsock;  // convert

protected:
	virtual void convert();
};


}  // namespace kumo

#endif /* logic/boot.h */

