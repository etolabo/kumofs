#ifndef MLOGGER_NULL_H__
#define MLOGGER_NULL_H__

#include "mlogger.h"

class mlogger_null : public mlogger {
public:
	mlogger_null();
	~mlogger_null();

	void log_impl(level lv, std::string& str);
};

#endif /* mlogger_null.h */

