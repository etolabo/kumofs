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
	std::string db_backup_basename;  // convert?

	unsigned short replicate_set_retry_num;
	unsigned short replicate_delete_retry_num;
	unsigned short replace_propose_retry_num;
	unsigned short replace_push_retry_num;

	virtual void convert()
	{
		cluster_addr = rpc::address(cluster_addr_in);
		cluster_lsock = scoped_listen_tcp::listen(cluster_addr);
		manager1 = rpc::address(manager1_in);
		manager2 = rpc::address(manager2_in);
		db_backup_basename = dbpath + "-";
		rpc_cluster_args::convert();
	}

	arg_t(int argc, char** argv) :
		replicate_set_retry_num(20),
		replicate_delete_retry_num(20),
		replace_propose_retry_num(20),
		replace_push_retry_num(20)
	{
		using namespace kazuhiki;
		set_basic_args();
		on("-l",  "--listen",
				type::connectable(&cluster_addr_in, SERVER_DEFAULT_PORT));
		on("-s", "--store",
				type::string(&dbpath));
		on("-m", "--manager1",
				type::connectable(&manager1_in, MANAGER_DEFAULT_PORT));
		on("-p", "--manager2", &manager2_set,
				type::connectable(&manager2_in, MANAGER_DEFAULT_PORT));
		on("-S", "--replicate-set-retry",
				type::numeric(&replicate_set_retry_num, replicate_set_retry_num));
		on("-G", "--replicate-delete-retry",
				type::numeric(&replicate_delete_retry_num, replicate_delete_retry_num));
		parse(argc, argv);
	}

	void show_usage()
	{
std::cout <<
"usage: "<<prog<<" -m <addr[:port="<<MANAGER_DEFAULT_PORT<<"]> -p <addr[:port"<<MANAGER_DEFAULT_PORT<<"]> -l <addr[:port="<<SERVER_DEFAULT_PORT<<"]> -s <path.tch>\n"
"\n"
"  -l  <addr[:port="<<SERVER_DEFAULT_PORT<<"]>   "        "--listen         listen address\n"
"  -s  <path.tch>            "                            "--store          path to database\n"
"  -m  <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>   "       "--manager1       address of manager 1\n"
"  -p  <addr[:port="<<MANAGER_DEFAULT_PORT<<"]>   "       "--manager2       address of manager 2\n"
"  -S  <number="<<replicate_set_retry_num<<">   "         "--replicate-set-retry    replicate set retry limit\n"
"  -D  <number="<<replicate_delete_retry_num<<">   "      "--replicate-delete-retry replicate delete retry limit\n"
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

	// daemonize
	if(!arg.pidfile.empty()) {
		do_daemonize(!arg.logfile.empty(), arg.pidfile.c_str());
	}

	// open database
	std::auto_ptr<Storage> db(new Storage(arg.dbpath.c_str()));
	arg.db = db.get();

	// run server
	Server::initialize(arg);
	Server::instance().run();
	Server::instance().join();

	return 0;
}

