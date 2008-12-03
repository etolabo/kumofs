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

	sockaddr_in ctlsock_addr_in;
	int ctlsock_lsock;

	virtual void convert()
	{
		cluster_addr = rpc::address(cluster_addr_in);
		cluster_lsock = scoped_listen_tcp::listen(cluster_addr);
		partner = rpc::address(partner_in);
		ctlsock_lsock = scoped_listen_tcp::listen(ctlsock_addr_in);
		rpc_cluster_args::convert();
	}

	arg_t(int argc, char** argv) :
		replace_delay_clocks(4)
	{
		using namespace kazuhiki;
		set_basic_args();
		on("-l",  "--listen",
				type::connectable(&cluster_addr_in, MANAGER_DEFAULT_PORT));
		on("-c", "--control",
				type::listenable(&ctlsock_addr_in, CONTROL_DEFAULT_PORT));
		on("-p", "--partner",  &partner_set,
				type::connectable(&partner_in, MANAGER_DEFAULT_PORT));
		on("-a", "--auto-replace",
				type::boolean(&auto_replace));
		on("-Rs", "--replace-delay",
				type::numeric(&replace_delay_clocks, replace_delay_clocks));
		parse(argc, argv);
	}

	void show_usage()
	{
std::cout <<
"usage: "<<prog<<" -l <addr[:port="<<MANAGER_DEFAULT_PORT<<"]> -p <addr[:port="<<MANAGER_DEFAULT_PORT<<"]> [-c port="<<CONTROL_DEFAULT_PORT<<"]\n"
"\n"
"  -p  <addr[:port="<<MANAGER_DEFAULT_PORT  <<"]>   "      "--partner        master-slave replication partner\n"
"  -c  <[addr:]port="<<CONTROL_DEFAULT_PORT <<">   "       "--control        dynamic control socket\n"
"  -a                        "                             "--auto-replace   enable auto replacing\n"
"  -Rs <number="<<replace_delay_clocks  <<">            "  "--replace-delay  delay steps of auto replacing\n"
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


