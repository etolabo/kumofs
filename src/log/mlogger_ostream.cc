#include "mlogger_ostream.h"
#include <string.h>

mlogger_ostream::mlogger_ostream(level runtime_level, std::ostream& stream) :
	mlogger(runtime_level),
	m_stream(stream)
{}

mlogger_ostream::~mlogger_ostream()
{}

void mlogger_ostream::log_impl(level lv, std::string& str)
{
	m_stream << str << std::endl;
}

