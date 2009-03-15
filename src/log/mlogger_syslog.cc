#include "mlogger_syslog.h"
#include <string.h>

mlogger_syslog::mlogger_syslog(level runtime_level, const char* ident, int facility, int option) :
	mlogger(runtime_level)
{
	::openlog(ident, option, facility);
}

mlogger_syslog::~mlogger_syslog()
{
	::closelog();
}

void mlogger_syslog::log_impl(level lv, std::string& str)
{
	int priority = LOG_DEBUG;
	switch(lv) {
	case TRACE:
	case DEBUG:
		priority = LOG_DEBUG;
		break;
	case INFO:
		priority = LOG_INFO;
		break;
	case WARN:
		priority = LOG_NOTICE;
		break;
	case ERROR:
		priority = LOG_ERR;
		break;
	case FATAL:
		priority = LOG_CRIT;
		break;
	}

	::syslog(priority, "%s", str.c_str());
}

