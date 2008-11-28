#include "logic/base.h"
#include "logic/gw.h"
#include "gateway/memproto_text.h"
#include <fstream>

using namespace kumo;

#define MEMPROTO_TEXT_DEFAULT_PORT 19788

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

	sockaddr_in memproto_text_addr_in;
	int memproto_text_lsock;  // convert

	virtual void convert()
	{
		manager1 = rpc::address(manager1_in);
		manager2 = rpc::address(manager2_in);
		memproto_text_lsock = scoped_listen_tcp::listen(memproto_text_addr_in);
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
				type::connectable(&manager1_in, CLUSTER_DEFAULT_PORT));
		on("-p", "--manager2", &manager2_set,
				type::connectable(&manager2_in, CLUSTER_DEFAULT_PORT));
		on("-t", "--memproto-text",
				type::listenable(&memproto_text_addr_in, MEMPROTO_TEXT_DEFAULT_PORT));
		on("-G", "--get-retry",
				type::numeric(&get_retry_num, get_retry_num));
		on("-S", "--get-retry",
				type::numeric(&set_retry_num, set_retry_num));
		on("-D", "--get-retry",
				type::numeric(&delete_retry_num, delete_retry_num));
		on("-rn", "--renew-threshold",
				type::numeric(&renew_threshold, renew_threshold));
		parse(argc, argv);
	}

	void show_usage()
	{
std::cout <<
"usage: "<<prog<<" -m <addr[:port]> -p <addr[:port]> [-c port="<<MEMPROTO_TEXT_DEFAULT_PORT<<"]\n"
"\n"
"  -m  <addr[:port="<<CLUSTER_DEFAULT_PORT<<"]>   "       "--manager1       address of manager 1\n"
"  -p  <addr[:port="<<CLUSTER_DEFAULT_PORT<<"]>   "       "--manager2       address of manager 2\n"
"  -t  <[addr:]port="<<MEMPROTO_TEXT_DEFAULT_PORT<<">   " "--memproto-text  memcached text protocol listen port\n"
"  -G  <number="<<get_retry_num<<"> "                     "--get-retry      get retry\n"
"  -S  <number="<<set_retry_num<<"> "                     "--set-retry      set retry\n"
"  -D  <number="<<delete_retry_num<<"> "                  "--delete-retry   delete retry\n"
"  -rn <number="<<renew_threshold<<"> "                   "--renew-threshold renew threshold\n"
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

	std::auto_ptr<MemprotoText> mpt(new MemprotoText(arg.memproto_text_lsock));

	Gateway::initialize(arg);
	Gateway::instance().add_gateway(mpt.get());
	Gateway::instance().run();
	Gateway::instance().join();
}

