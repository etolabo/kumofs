#include "logic/base.h"
#include "logic/mgr.h"
#include <fstream>

using namespace kumo;

struct arg_t : rpc_cluster_args {
	unsigned short replace_delay_clocks;

	bool auto_replace;

	bool partner_set;
	struct sockaddr_in partner_in;
	rpc::address partner;  // convert

	double keepalive_interval;  // sec
	unsigned long keepalive_interval_usec;  // convert

	sockaddr_in clisrv_addr_in;
	int clisrv_lsock;  // convert

	sockaddr_in ctlsock_addr_in;
	int ctlsock_lsock;

	virtual void convert()
	{
		partner = rpc::address(partner_in);
		keepalive_interval_usec = keepalive_interval *1000 *1000;
		clisrv_lsock = scoped_listen_tcp::listen(clisrv_addr_in);
		ctlsock_lsock = scoped_listen_tcp::listen(ctlsock_addr_in);
		rpc_cluster_args::convert();
	}

	arg_t(int argc, char** argv) :
		replace_delay_clocks(4),
		keepalive_interval(2)
	{
		using namespace kazuhiki;
		set_basic_args();
		on("-c", "--client",
				type::listenable(&clisrv_addr_in, CLIENT_DEFAULT_PORT));
		on("-a", "--admin",
				type::listenable(&ctlsock_addr_in, CONTROL_DEFAULT_PORT));
		on("-p", "--partner",  &partner_set,
				type::connectable(&partner_in, CLUSTER_DEFAULT_PORT));
		on("-a", "--auto-replace",
				type::boolean(&auto_replace));
		on("-SR", "--replace-delay-steps",
				type::numeric(&replace_delay_clocks, replace_delay_clocks));
		on("-K", "--keepalive-interval",
				type::numeric(&keepalive_interval, keepalive_interval));
		parse(argc, argv);
	}

	void show_usage()
	{
std::cout <<
"usage: "<<prog<<" -l <addr[:port]> -p <addr[:port]>\n"
"\n"
"  -c  <path.tch>    --database      database path name\n"
"  -p  <addr[:port="<<CLUSTER_DEFAULT_PORT<<"]>   --partner       address of master-slave replication partner\n"
"  -a  <[addr:]port="<<CONTROL_DEFAULT_PORT<<">   --admin       dynamic control socket\n"
"  -a                --auto-replace  enable auto replacing\n"
"  -SR <number="<<replace_delay_clocks<<">     --replace-delay-clocks    delay steps of auto replacing\n"
"  -K  <number="<<keepalive_interval<<">      --keepalive-interval   keepalive interval in seconds.\n"
;
rpc_cluster_args::show_usage();
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

	Manager::initialize(arg);
	Manager::instance().run();
	Manager::instance().join();

	return 0;
}


