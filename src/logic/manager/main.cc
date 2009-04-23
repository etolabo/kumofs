#include "logic/boot.h"
#include "manager/framework.h"
#include "manager/init.h"

using namespace kumo;

struct arg_t : cluster_args {
	unsigned short replace_delay_seconds;

	bool auto_replace;

	bool partner_set;
	struct sockaddr_in partner_in;
	rpc::address partner;  // convert

	virtual void convert()
	{
		cluster_args::convert();
		partner = rpc::address(partner_in);
	}

	arg_t(int argc, char** argv) :
		replace_delay_seconds(4)
	{
		using namespace kazuhiki;
		set_basic_args();
		on("-l",  "--listen",
				type::connectable(&cluster_addr_in, MANAGER_DEFAULT_PORT));
		on("-p", "--partner",  &partner_set,
				type::connectable(&partner_in, MANAGER_DEFAULT_PORT));
		on("-a", "--auto-replace",
				type::boolean(&auto_replace));
		on("-Rs", "--replace-delay",
				type::numeric(&replace_delay_seconds, replace_delay_seconds));
		parse(argc, argv);
	}

	void show_usage()
	{
		std::cout <<
		"usage: "<<prog<<
				" -l <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>"
				" -p <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>\n"
		"\n"
		"  -p  <addr[:port="<<MANAGER_DEFAULT_PORT  <<"]>   "
			"--partner        master-slave replication partner\n"
		"  -a                        "
			"--auto-replace   enable auto replacing\n"
		"  -Rs <number="<<replace_delay_seconds  <<">            "
			"--replace-delay  delay time of auto replacing in sec.\n"
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

	// run server
	manager::init(arg);
	manager::net->run(arg);
	manager::net->join();

	return 0;
}

