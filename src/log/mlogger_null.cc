#include "mlogger.h"
#include <string.h>

mlogger_null::mlogger_null() :
	mlogger((level)((int)FATAL+1))
{}

mlogger_null::~mlogger_null()
{}

void mlogger_null::log_impl(level lv, std::string& str)
{ }

