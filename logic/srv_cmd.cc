#include "logic/base.h"
#include "logic/srv.h"
#include <fstream>

using namespace kumo;

struct arg_t : rpc_cluster_args {

	std::string dbpath;

	sockaddr_in manager1_in;
	sockaddr_in manager2_in;
	bool manager2_set;
	rpc::address manager1;  // convert
	rpc::address manager2;  // convert

	Storage* db;

	virtual void convert()
	{
		manager1 = rpc::address(manager1_in);
		manager2 = rpc::address(manager2_in);
		rpc_cluster_args::convert();
	}

	arg_t(int argc, char** argv)
	{
		using namespace kazuhiki;
		set_basic_args();
		on("-s", "--store",
				type::string(&dbpath));
		on("-m", "--manager1",
				type::connectable(&manager1_in, CLUSTER_DEFAULT_PORT));
		on("-p", "--manager2", &manager2_set,
				type::connectable(&manager2_in, CLUSTER_DEFAULT_PORT));
		parse(argc, argv);
	}

	void show_usage()
	{
std::cout <<
"usage: "<<prog<<" -m <addr[:port="<<CLUSTER_DEFAULT_PORT<<"]> -p <addr[:port"<<CLUSTER_DEFAULT_PORT<<"]> -l <addr[:port="<<CLUSTER_DEFAULT_PORT<<"]> -s <path.tch>\n"
"\n"
"  -s  <path.tch>            "                            "--store          path to database\n"
"  -m  <addr[:port="<<CLUSTER_DEFAULT_PORT<<"]>   "       "--manager1       address of manager 1\n"
"  -p  <addr[:port="<<CLUSTER_DEFAULT_PORT<<"]>   "       "--manager2       address of manager 2\n"
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

	std::auto_ptr<Storage> db(new Storage(arg.dbpath.c_str()));
	arg.db = db.get();

	Server::initialize(arg);
	Server::instance().run();
	Server::instance().join();

	return 0;
}

