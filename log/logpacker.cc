#include "logpacker.h"

void logpacker::initialize(const char* fname)
{
	s_instance.reset(new logpack(fname));
}

void logpacker::destroy()
{
	s_instance.reset();
}

std::auto_ptr<logpack> logpacker::s_instance;

