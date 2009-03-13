#include "logic/boot.h"
#include "gateway/framework.h"
#include "gateway/init.h"
#include "gateway/gate_memtext.h"
#include "gateway/gate_memproto.h"
#include "gateway/gate_cloudy.h"

using namespace kumo;

#define MEMTEXT_DEFAULT_PORT  11411
#define MEMPROTO_DEFAULT_PORT 11511
#define CLOUDY_DEFAULT_PORT   11611

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

	bool async_replicate_set;
	bool async_replicate_delete;

	bool memtext_set;
	sockaddr_in memtext_addr_in;
	int memtext_lsock;  // convert

	bool memproto_set;
	sockaddr_in memproto_addr_in;
	int memproto_lsock;  // convert

	bool cloudy_set;
	sockaddr_in cloudy_addr_in;
	int cloudy_lsock;  // convert

	virtual void convert()
	{
		manager1 = rpc::address(manager1_in);
		manager2 = rpc::address(manager2_in);

		if(!memtext_set && !memproto_set && !cloudy_set) {
			throw std::runtime_error("-t, -b or -c is required");
		}
		if(memtext_set) {
			memtext_lsock = scoped_listen_tcp::listen(memtext_addr_in);
		}
		if(memproto_set) {
			memproto_lsock = scoped_listen_tcp::listen(memproto_addr_in);
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
		renew_threshold(4)
	{
		using namespace kazuhiki;
		set_basic_args();
		on("-m", "--manager1",
				type::connectable(&manager1_in, MANAGER_DEFAULT_PORT));
		on("-p", "--manager2", &manager2_set,
				type::connectable(&manager2_in, MANAGER_DEFAULT_PORT));
		on("-t", "--memproto-text", &memtext_set,
				type::listenable(&memtext_addr_in, MEMTEXT_DEFAULT_PORT));
		on("-b", "--memproto-binary", &memproto_set,
				type::listenable(&memproto_addr_in, MEMPROTO_DEFAULT_PORT));
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
		on("-As", "--async-replicate-set",
				type::boolean(&async_replicate_set));
		on("-Ad", "--async-replicate-delete",
				type::boolean(&async_replicate_delete));
		parse(argc, argv);
	}

	void show_usage()
	{
std::cout <<
"usage: "<<prog<<" -m <addr[:port]> -p <addr[:port]> [-t port="<<MEMTEXT_DEFAULT_PORT<<"]\n"
"\n"
"  -m  <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>   "       "--manager1        address of manager 1\n"
"  -p  <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>   "       "--manager2        address of manager 2\n"
"  -t  <[addr:]port="<<MEMTEXT_DEFAULT_PORT<<">   "       "--memproto-text   memcached text protocol listen port\n"
"  -b  <[addr:]port="<<MEMPROTO_DEFAULT_PORT<<">   "      "--memprpto-binary memcached binary protocol listen port\n"
"  -c  <[addr:]port="<<CLOUDY_DEFAULT_PORT<<">   "        "--cloudy          asynchronous memcached binary protocol listen port\n"
"  -As               "                                    "--async-replicate-set    send response without waiting replication on set\n"
"  -Ad               "                                    "--async-replicate-delete send response without waiting replication on delete\n"
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
	init_mlogger(arg.logfile, arg.pidfile.empty(), loglevel);

	// initialize gate
	std::auto_ptr<Memtext> mpt;
	std::auto_ptr<Memproto> mpb;
	std::auto_ptr<Cloudy> cl;
	if(arg.memtext_set)  { mpt.reset(new Memtext(arg.memtext_lsock));   }
	if(arg.memproto_set) { mpb.reset(new Memproto(arg.memproto_lsock)); }
	if(arg.cloudy_set)   { cl.reset(new Cloudy(arg.cloudy_lsock));      }

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

	if(mpt.get()) { gateway::add_gate(mpt.get()); }
	if(mpb.get()) { gateway::add_gate(mpb.get()); }
	if(cl.get())  { gateway::add_gate(cl.get());  }

	gateway::net->run();
	gateway::net->join();
}

