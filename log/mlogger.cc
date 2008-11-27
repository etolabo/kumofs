#include "mlogger.h"

mlogger* mlogger::s_logger;

void mlogger::reset(mlogger* lg)
{
	if(s_logger) { delete s_logger; }
	s_logger = lg;
}

void mlogger::destroy()
{
	delete s_logger;
	s_logger = NULL;
}


mlogger::mlogger(level runtime_level) :
	m_runtime_level(runtime_level) {}

mlogger::~mlogger() {}
