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
#include "gateway/framework.h"
#include "gateway/init.h"
#include "gate/memcache_text.h"
#include "gate/memcache_binary.h"
#include "gate/cloudy.h"

using namespace kumo;

#define MEMTEXT_DEFAULT_PORT  11411
#define MEMPROTO_DEFAULT_PORT 11511
#define CLOUDY_DEFAULT_PORT   11611

struct arg_t : rpc_args {

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

	unsigned short get_retry_num;
	unsigned short set_retry_num;
	unsigned short delete_retry_num;

	unsigned short renew_threshold;

	bool async_replicate_set;
	bool async_replicate_delete;

	std::string local_cache;

	bool mctext_set;
#ifdef KUMO_IPV6
	sockaddr_in6 mctext_addr_in;
#else
	sockaddr_in mctext_addr_in;
#endif
	int mctext_lsock;  // convert

	bool mcbin_set;
#ifdef KUMO_IPV6
	sockaddr_in6 mcbin_addr_in;
#else
	sockaddr_in mcbin_addr_in;
#endif
	int mcbin_lsock;  // convert

	bool cloudy_set;
#ifdef KUMO_IPV6
	sockaddr_in6 cloudy_addr_in;
#else
	sockaddr_in cloudy_addr_in;
#endif
	int cloudy_lsock;  // convert

	bool mc_save_flag;
	bool mc_save_exptime;

	std::string key_prefix;

	virtual void convert()
	{
		rpc_args::convert();

		manager1 = rpc::address(manager1_in);
		manager2 = rpc::address(manager2_in);

		if(!mctext_set && !mcbin_set && !cloudy_set) {
			throw std::runtime_error("-t, -b or -c is required");
		}
		if(mctext_set) {
			mctext_lsock = scoped_listen_tcp::listen(mctext_addr_in);
		}
		if(mcbin_set) {
			mcbin_lsock = scoped_listen_tcp::listen(mcbin_addr_in);
		}
		if(cloudy_set) {
			cloudy_lsock = scoped_listen_tcp::listen(cloudy_addr_in);
		}
	}

	arg_t(int& argc, char* argv[]) :
		get_retry_num(5),
		set_retry_num(20),
		delete_retry_num(20),
		renew_threshold(4)
	{
		using namespace kazuhiki;
		set_basic_args();
		on("-m", "--manager1",
				type::connectable(&manager1_in, MANAGER_DEFAULT_PORT));
		on("-p", "--manager2", &manager2_set,
				type::connectable(&manager2_in, MANAGER_DEFAULT_PORT));
		on("-lc","--local-cache",
				type::string(&local_cache, ""));
		on("-t", "--memproto-text", &mctext_set,
				type::listenable(&mctext_addr_in, MEMTEXT_DEFAULT_PORT));
		on("-b", "--memproto-binary", &mcbin_set,
				type::listenable(&mcbin_addr_in, MEMPROTO_DEFAULT_PORT));
		on("-c", "--cloudy", &cloudy_set,
				type::listenable(&cloudy_addr_in, CLOUDY_DEFAULT_PORT));
		on("-F", "--memproto-save-flag",
				type::boolean(&mc_save_flag));
		on("-E", "--memproto-save-expire",
				type::boolean(&mc_save_exptime));
		on("-G", "--get-retry",
				type::numeric(&get_retry_num, get_retry_num));
		on("-S", "--set-retry",
				type::numeric(&set_retry_num, set_retry_num));
		on("-D", "--delete-retry",
				type::numeric(&delete_retry_num, delete_retry_num));
		on("-rn", "--renew-threshold",
				type::numeric(&renew_threshold, renew_threshold));
		on("-As", "--async-replicate-set",
				type::boolean(&async_replicate_set));
		on("-Ad", "--async-replicate-delete",
				type::boolean(&async_replicate_delete));
		on("-k", "--key-prefix",
				type::string(&key_prefix, ""));
		parse(argc, argv);
	}

	void show_usage()
	{
		std::cout <<
		"usage: "<<prog<<
				" -m <addr[:port]>"
				" -p <addr[:port]>"
				" [-t port] [-b port] [-c port]\n"
		"\n"
		"  -m  <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>   "
			"--manager1        address of manager 1\n"
		"  -p  <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>   "
			"--manager2        address of manager 2\n"
		"  -lc                       "
			"--local-cache     local cache (Tokyo Cabinet abstract database)\n"
		"  -t  <[addr:]port="<<MEMTEXT_DEFAULT_PORT<<">   "
			"--memproto-text   memcached text protocol listen port\n"
		"  -b  <[addr:]port="<<MEMPROTO_DEFAULT_PORT<<">   "
			"--memproto-binary memcached binary protocol listen port\n"
		"  -c  <[addr:]port="<<CLOUDY_DEFAULT_PORT<<">   "
			"--cloudy          asynchronous memcached binary protocol listen port\n"
		"  -F                "
			"--memproto-save-flag     save flags on memcached protocol\n"
		"  -E                "
			"--memproto-save-expire   save expire time on memcached protocol\n"
		"  -As               "
			"--async-replicate-set    send response without waiting replication on set\n"
		"  -Ad               "
			"--async-replicate-delete send response without waiting replication on delete\n"
		"  -G  <number="<<get_retry_num<<">    "
			"--get-retry              get retry limit\n"
		"  -S  <number="<<set_retry_num<<">   "
			"--set-retry              set retry limit\n"
		"  -D  <number="<<delete_retry_num<<">   "
			"--delete-retry           delete retry limit\n"
		"  -rn <number="<<renew_threshold<<">    "
			"--renew-threshold        hash space renew threshold\n"
		"  -k <string>       "
			"--key-prefix             add prefix to keys automatically\n"
		;
		rpc_args::show_usage();
	}

};



int main(int argc, char* argv[])
{
	arg_t arg(argc, argv);

	// initialize logger first
	mlogger::level loglevel = (arg.verbose ? mlogger::TRACE : mlogger::WARN);
	init_mlogger(arg.logfile, arg.pidfile.empty(), loglevel);

	// initialize gate
	std::auto_ptr<MemcacheText> mctext;
	std::auto_ptr<MemcacheBinary> mcbin;
	std::auto_ptr<Cloudy> cloudy;
	if(arg.mctext_set) { mctext.reset(new MemcacheText(arg.mctext_lsock, arg.mc_save_flag, arg.mc_save_exptime)); }
	if(arg.mcbin_set)  { mcbin.reset(new MemcacheBinary(arg.mcbin_lsock, arg.mc_save_flag, arg.mc_save_exptime)); }
	if(arg.cloudy_set) { cloudy.reset(new Cloudy(arg.cloudy_lsock)); }

	// daemonize
	if(!arg.pidfile.empty()) {
		do_daemonize(!arg.logfile.empty(), arg.pidfile.c_str());
	}

	// initialize binary logger
	if(arg.logpack_path_set) {
		logpacker::initialize(arg.logpack_path.c_str());
	}

	// run server
	gateway::init(arg);

	if(mctext.get()) { mctext->run(); }
	if(mcbin.get())  { mcbin->run();  }
	if(cloudy.get()) { cloudy->run(); }

	gateway::net->run(arg);
	gateway::net->join();
}

