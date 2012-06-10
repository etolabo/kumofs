//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
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
#include "logic/boot.h"
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <mp/utility.h>
#include <fstream>

namespace kumo {


scoped_listen_tcp::scoped_listen_tcp(struct sockaddr_in addr) :
	m_addr(addr),
	m_sock(listen(m_addr)) { }

#ifdef KUMO_IPV6
scoped_listen_tcp::scoped_listen_tcp(struct sockaddr_in6 addr) :
	m_addr(addr),
	m_sock(listen(m_addr)) { }
#endif

scoped_listen_tcp::~scoped_listen_tcp()
{
	::close(m_sock);
}


int scoped_listen_tcp::listen(const rpc::address& addr)
{
#ifdef KUMO_IPV6
	int lsock = socket(PF_INET6, SOCK_STREAM, 0);
#else
	int lsock = socket(PF_INET, SOCK_STREAM, 0);
#endif
	if(lsock < 0) {
		throw std::runtime_error("socket failed");
	}

	int on = 1;
	if( ::setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0 ) {
		::close(lsock);
		throw std::runtime_error("setsockopt failed");
	}

	char addrbuf[addr.addrlen()];
	addr.getaddr((sockaddr*)addrbuf);

	if( ::bind(lsock, (sockaddr*)addrbuf, sizeof(addrbuf)) < 0 ) {
		::close(lsock);
		throw std::runtime_error("bind failed");
	}

	if( ::listen(lsock, 1024) < 0 ) {
		::close(lsock);
		throw std::runtime_error("listen failed");
	}

	mp::set_nonblock(lsock);

	return lsock;
}


void do_daemonize(bool close_stdio, const char* pidfile)
{
	pid_t pid;
	pid = fork();
	if(pid < 0) { perror("fork"); exit(1); }
	if(pid != 0) { exit(0); }
	if(setsid() == -1) { perror("setsid"); exit(1); }
	pid = fork();
	if(pid < 0) { perror("fork"); exit(1); }
	if(pid != 0) { exit(0); }
	if(pidfile) {
		FILE* f = fopen(pidfile, "w");
		if(!f) { perror("can't open pid file"); exit(1); }
		fprintf(f, "%d", getpid());
		fclose(f);
	}
	if(close_stdio) {
		int devnull_r = open("/dev/null", O_RDONLY);
		if(devnull_r < 0) { perror("open(\"/dev/null\", \"r\")"); exit(1); }
		int devnull_a = open("/dev/null", O_APPEND);
		if(devnull_a < 0) { perror("open(\"/dev/null\"), \"a\""); exit(1); }
		close(0);
		close(1);
		close(2);
		if(dup2(devnull_r, 0) < 0) { perror("dup2"); exit(1); }
		if(dup2(devnull_a, 1) < 0) { perror("dup2"); exit(1); }
		if(dup2(devnull_a, 2) < 0) { perror("dup2"); exit(1); }
		close(devnull_r);
		close(devnull_a);
	}
}


void init_mlogger(const std::string& logfile, bool use_tty, mlogger::level level)
{
	if(!logfile.empty()) {
		// log to file
		if(logfile == "-") {
			mlogger::reset(new mlogger_ostream(level, std::cout));
		} else {
			std::ostream* logstream = new std::ofstream(logfile.c_str(), std::ios::app);
			mlogger::reset(new mlogger_ostream(level, *logstream));
		}
	} else if(use_tty) {
		// log to tty
		mlogger::reset(new mlogger_tty(level, std::cout));
	} else {
		// log to stdout
		mlogger::reset(new mlogger_ostream(level, std::cout));
	}
}


rpc_args::rpc_args() :
	keepalive_interval(2.0),
	clock_interval(2.0),
	connect_timeout_sec(10.0),
	connect_retry_limit(4),
	wthreads(2),
	rthreads(8)
{
	kazuhiki::init();
}

rpc_args::~rpc_args() { }

cluster_args::cluster_args() :
	cluster_lsock(-1) { }

cluster_args::~cluster_args()
{
	::close(cluster_lsock);
}


void rpc_args::convert()
{
	keepalive_interval_usec = keepalive_interval *1000 *1000;
	clock_interval_usec = clock_interval * 1000 * 1000;
	connect_timeout_msec = connect_timeout_sec * 1000;
}

void cluster_args::convert()
{
	cluster_addr = rpc::address(cluster_addr_in);
#ifdef KUMO_IPV6
	// cluster_addr_in.sin6_addr = in6addr_any;  // listen any
#else
	// cluster_addr_in.sin_addr.s_addr = INADDR_ANY;  // listen any
#endif
	cluster_lsock = scoped_listen_tcp::listen(
			rpc::address(cluster_addr_in));
	rpc_args::convert();
}


void rpc_args::set_basic_args()
{
	using namespace kazuhiki;
	on("-v", "--verbose",
			type::boolean(&verbose));
	on("-o", "--log", &logfile_set,
			type::string(&logfile));
	on("-g", "--binary-log", &logpack_path_set,
			type::string(&logpack_path));
	on("-d", "--daemon", &pidfile_set,
			type::string(&pidfile));
	on("-k", "--keepalive-interval",
			type::numeric(&keepalive_interval_usec, keepalive_interval_usec));
	on("-Ci", "--clock-interval",
			type::numeric(&clock_interval, clock_interval));
	on("-Ys", "--connect-timeout",
			type::numeric(&connect_timeout_sec, connect_timeout_sec));
	on("-Yn", "--connect-retry-limit",
			type::numeric(&connect_retry_limit, connect_retry_limit));
	on("-TW", "--write-threads",
			type::numeric(&wthreads, wthreads));
	on("-TR", "--read-threads",
			type::numeric(&rthreads, rthreads));
}

void rpc_args::show_usage()
{
	std::cout <<
		"  -k  <number="<<keepalive_interval<<">    "
			"--keepalive-interval     keepalive interval in seconds\n"
		"  -Ys <number="<<connect_timeout_sec<<">   "
			"--connect-timeout        connect timeout time in seconds\n"
		"  -Yn <number="<<connect_retry_limit<<">    "
			"--connect-retry-limit    connect retry limit\n"
		"  -Ci <number="<<clock_interval<<">    "
			"--clock-interval         clock interval in seconds\n"
		"  -TW <number="<<wthreads<<">    "
			"--write-threads          number of threads for asynchronous writing\n"
		"  -TR <number="<<rthreads<<">    "
			"--read-threads           number of threads for asynchronous reading\n"
		"  -o  <path.log>    "
			"--log                    output logs to the file\n"
		"  -g  <path.mpac>   "
			"--binary-log             enable binary log\n"
		"  -v                "
			"--verbose\n"
		"  -d  <path.pid>    "
			"--daemon\n"
#ifdef VERSION
#ifdef REVISION
	"\n  " "v" VERSION " revision " REVISION "\n"
#else
	"\n  " "v" VERSION "\n"
#endif
#endif
	<< std::endl;
}

void cluster_args::set_basic_args()
{
	rpc_args::set_basic_args();
}

void cluster_args::show_usage()
{
	rpc_args::show_usage();
}

void rpc_args::parse(int argc, char** argv)
try {
	prog = argv[0];
	--argc;
	++argv;
	kazuhiki::break_order(argc, argv);

	convert();

} catch (std::runtime_error& e) {
	show_usage();
	std::cerr << "error: " << e.what() << std::endl;
	exit(1);
}


}  // namespace kumo

