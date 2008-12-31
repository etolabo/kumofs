#include "logic/base.h"
#include "logic/gw.h"
#include "gateway/memtext.h"
#include "gateway/cloudy.h"
#include <fstream>

using namespace kumo;

#define MEMTEXT_DEFAULT_PORT 19799
#define CLOUDY_DEFAULT_PORT 19798

struct arg_t : rpc_server_args {

	sockaddr_in manager1_in;
	sockaddr_in manager2_in;
	bool manager2_set;
	rpc::address manager1;  // convert
	rpc::address manager2;  // convert

	unsigned short get_retry_num;
	unsigned short set_retry_num;
	unsigned short delete_retry_num;

	unsigned short renew_threshold;

	//bool memtext_set;
	sockaddr_in memtext_addr_in;
	int memtext_lsock;  // convert

	bool cloudy_set;
	sockaddr_in cloudy_addr_in;
	int cloudy_lsock;  // convert

	virtual void convert()
	{
		manager1 = rpc::address(manager1_in);
		manager2 = rpc::address(manager2_in);
		//if(!memtext_set && !cloudy_set) {
		//	throw std::runtime_error("memproto text or cloudy must be set");
		//}
		//if(memtext_set) {
		//	memtext_lsock = scoped_listen_tcp::listen(memtext_addr_in);
		//}
		if(!cloudy_set) {
			memtext_lsock = scoped_listen_tcp::listen(memtext_addr_in);
		}
		if(cloudy_set) {
			cloudy_lsock = scoped_listen_tcp::listen(cloudy_addr_in);
		}
		rpc_server_args::convert();
	}

	arg_t(int& argc, char* argv[]) :
		get_retry_num(5),
		set_retry_num(20),
		delete_retry_num(20),
		renew_threshold(5)
	{
		using namespace kazuhiki;
		set_basic_args();
		on("-m", "--manager1",
				type::connectable(&manager1_in, MANAGER_DEFAULT_PORT));
		on("-p", "--manager2", &manager2_set,
				type::connectable(&manager2_in, MANAGER_DEFAULT_PORT));
		on("-t", "--memproto-text",// &memtext_set,
				type::listenable(&memtext_addr_in, MEMTEXT_DEFAULT_PORT));
		on("-c", "--cloudy", &cloudy_set,
				type::listenable(&cloudy_addr_in, CLOUDY_DEFAULT_PORT));
		on("-G", "--get-retry",
				type::numeric(&get_retry_num, get_retry_num));
		on("-S", "--set-retry",
				type::numeric(&set_retry_num, set_retry_num));
		on("-D", "--delete-retry",
				type::numeric(&delete_retry_num, delete_retry_num));
		on("-rn", "--renew-threshold",
				type::numeric(&renew_threshold, renew_threshold));
		parse(argc, argv);
	}

	void show_usage()
	{
std::cout <<
"usage: "<<prog<<" -m <addr[:port]> -p <addr[:port]> [-t port="<<MEMTEXT_DEFAULT_PORT<<"]\n"
"\n"
"  -m  <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>   "       "--manager1       address of manager 1\n"
"  -p  <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>   "       "--manager2       address of manager 2\n"
"  -t  <[addr:]port="<<MEMTEXT_DEFAULT_PORT<<">   " "--memproto-text  memcached text protocol listen port\n"
"  -c  <[addr:]port="<<CLOUDY_DEFAULT_PORT<<">   "        "--cloudy         memcached binary protocol listen port\n"
"  -G  <number="<<get_retry_num<<">    "                  "--get-retry              get retry limit\n"
"  -S  <number="<<set_retry_num<<">   "                   "--set-retry              set retry limit\n"
"  -D  <number="<<delete_retry_num<<">   "                "--delete-retry           delete retry limit\n"
"  -rn <number="<<renew_threshold<<">    "                "--renew-threshold        hash space renew threshold\n"
;
rpc_server_args::show_usage();
	}

};

int main(int argc, char* argv[])
{
	arg_t arg(argc, argv);

	// initialize logger first
	mlogger::level loglevel = (arg.verbose ? mlogger::TRACE : mlogger::WARN);
	if(arg.logfile_set) {
		// log to file
		std::ostream* logstream = new std::ofstream(arg.logfile.c_str(), std::ios::app);
		mlogger::reset(new mlogger_ostream(loglevel, *logstream));
	} else if(arg.pidfile_set) {
		// log to stdout
		mlogger::reset(new mlogger_ostream(loglevel, std::cout));
	} else {
		// log to tty
		mlogger::reset(new mlogger_tty(loglevel, std::cout));
	}

	// initialize memcache gateway
	std::auto_ptr<MemprotoText> mpt;
	std::auto_ptr<Cloudy> cl;
	//if(arg.memtext_set) {
	//	mpt.reset(new MemprotoText(arg.memtext_lsock));
	//}
	if(arg.cloudy_set) {
		cl.reset(new Cloudy(arg.cloudy_lsock));
	} else {
		mpt.reset(new MemprotoText(arg.memtext_lsock));
	}

	// daemonize
	if(!arg.pidfile.empty()) {
		do_daemonize(!arg.logfile.empty(), arg.pidfile.c_str());
	}

	// run server
	Gateway::initialize(arg);
	//if(arg.memtext_set) {
	//	Gateway::instance().add_gateway(mpt.get());
	//}
	if(arg.cloudy_set) {
		Gateway::instance().add_gateway(cl.get());
	} else {
		Gateway::instance().add_gateway(mpt.get());
	}
	Gateway::instance().run();
	Gateway::instance().join();
}

