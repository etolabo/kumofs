#ifndef MLOGGER_SYSLOG_H__
#define MLOGGER_SYSLOG_H__

#include "mlogger.h"
#include <syslog.h>

class mlogger_syslog : public mlogger {
public:
	mlogger_syslog(level runtime_level, const char* ident, int facility = LOG_USER, int option = 0);
	~mlogger_syslog();

	void log_impl(level lv, std::string& str);
};

#endif /* mlogger_syslog.h */

