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
#include "server/framework.h"
#include "server/init.h"

using namespace kumo;

struct arg_t : cluster_args {

	std::string dbpath;

#ifdef KUMO_IPV6
	sockaddr_in6 manager1_in;
	sockaddr_in6 manager2_in;
#else
	sockaddr_in manager1_in;
	sockaddr_in manager2_in;
#endif
	bool manager2_set;
	rpc::address manager1;  // convert
	rpc::address manager2;  // convert

	uint16_t stream_port;
	rpc::address stream_addr;  // convert
	int stream_lsock;

	std::string offer_tmpdir;

	Storage* db;
	std::string db_backup_basename;  // convert?

	unsigned short replicate_set_retry_num;
	unsigned short replicate_delete_retry_num;
	unsigned short replace_set_limit_mem;

	unsigned int garbage_min_time_sec;
	unsigned int garbage_max_time_sec;
	size_t garbage_mem_limit_kb;

	virtual void convert()
	{
		cluster_args::convert();

		stream_addr = cluster_addr;
		stream_addr.set_port(stream_port);
		stream_lsock = scoped_listen_tcp::listen(stream_addr);

		manager1 = rpc::address(manager1_in);
		if(manager2_set) {
			manager2 = rpc::address(manager2_in);
			if(manager2 == manager1) {
				throw std::runtime_error("-m and -p must be different");
			}
		}

		db_backup_basename = dbpath + "-";

		if(garbage_min_time_sec > garbage_max_time_sec) {
			garbage_min_time_sec = garbage_max_time_sec;
		}
	}

	arg_t(int argc, char** argv) :
		stream_port(SERVER_STREAM_DEFAULT_PORT),
		replicate_set_retry_num(20),
		replicate_delete_retry_num(20),
		replace_set_limit_mem(0),
		garbage_min_time_sec(60),
		garbage_max_time_sec(60*60),
		garbage_mem_limit_kb(2*1024)
	{
		clock_interval = 8.0;

		using namespace kazuhiki;
		set_basic_args();
		on("-l",  "--listen",
				type::connectable(&cluster_addr_in, SERVER_DEFAULT_PORT));
		on("-L", "--stream-listen",
				type::numeric(&stream_port, stream_port));
		on("-f", "--offer-tmp",
				type::string(&offer_tmpdir, "/tmp"));
		on("-s", "--store",
				type::string(&dbpath));
		on("-m", "--manager1",
				type::connectable(&manager1_in, MANAGER_DEFAULT_PORT));
		on("-p", "--manager2", &manager2_set,
				type::connectable(&manager2_in, MANAGER_DEFAULT_PORT));
		on("-S", "--replicate-set-retry",
				type::numeric(&replicate_set_retry_num, replicate_set_retry_num));
		on("-D", "--replicate-delete-retry",
				type::numeric(&replicate_delete_retry_num, replicate_delete_retry_num));
		on("-M", "--replace-memory-limit",
				type::numeric(&replace_set_limit_mem, replace_set_limit_mem));
		on("-gN", "--garbage-min-time",
				type::numeric(&garbage_min_time_sec, garbage_min_time_sec));
		on("-gX", "--garbage-max-time",
				type::numeric(&garbage_max_time_sec, garbage_max_time_sec));
		on("-gS", "--garbage-mem-limit",
				type::numeric(&garbage_mem_limit_kb, garbage_mem_limit_kb));
		parse(argc, argv);
	}

	void show_usage()
	{
		std::cout <<
		"usage: "<<prog<<
					" -m <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>"
					" -p <addr[:port"<<MANAGER_DEFAULT_PORT<<"]>"
					" -l <addr[:port="<<SERVER_DEFAULT_PORT<<"]>"
					" -s <path.tch>\n"
		"\n"
		"  -l  <addr[:port="<<SERVER_DEFAULT_PORT<<"]>   "
			"--listen         listen address\n"
		"  -L  <port="<<SERVER_STREAM_DEFAULT_PORT<<">          "
			"--stream-listen  listen port for replacing stream\n"
		"  -f  <dir="<<"/tmp"<<">            "
			"--offer-tmp      path to temporary directory for replacing\n"
		"  -s  <path.tch>            "
			"--store          path to database\n"
		"  -m  <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>   "
			"--manager1       address of manager 1\n"
		"  -p  <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>   "
			"--manager2       address of manager 2\n"
		"  -S  <number="<<replicate_set_retry_num<<">        "
			"--replicate-set-retry    replicate set retry limit\n"
		"  -D  <number="<<replicate_delete_retry_num<<">        "
			"--replicate-delete-retry replicate delete retry limit\n"
		"  -M  <number="<<replace_set_limit_mem<<">        "
			"--replace-memory-limit   Memory map limit size\n"
		"  -gN <seconds="<<garbage_min_time_sec<<">       "
			"--garbage-min-time       minimum time to maintenance deleted key\n"
		"  -gX <seconds="<<garbage_max_time_sec<<">     "
			"--garbage-max-time       maximum time to maintenance deleted key\n"
		"  -gS <kilobytes="<<garbage_mem_limit_kb<<">   "
			"--garbage-mem-limit      maximum memory usage to memory deleted key\n"
		;
		cluster_args::show_usage();
	}
};

int main(int argc, char* argv[])
{
	arg_t arg(argc, argv);

	// initialize logger first
	mlogger::level loglevel = (arg.verbose ? mlogger::TRACE : mlogger::WARN);
	init_mlogger(arg.logfile, arg.pidfile.empty(), loglevel);

	// daemonize
	if(!arg.pidfile.empty()) {
		do_daemonize(!arg.logfile.empty(), arg.pidfile.c_str());
	}

	// initialize binary logger
	if(arg.logpack_path_set) {
		logpacker::initialize(arg.logpack_path.c_str());
	}

	// open database
	std::auto_ptr<Storage> db(
			new Storage(arg.dbpath.c_str(),
				arg.garbage_min_time_sec,
				arg.garbage_max_time_sec,
				arg.garbage_mem_limit_kb*1024));
	arg.db = db.get();

	// run server
	server::init(arg);
	server::net->run(arg);
	server::net->join();

	return 0;
}

