#include "logpacker.h"

void logpacker::initialize(const std::string& basename, size_t lotate_size)
{
	s_instance.reset(new logpack(basename, lotate_size));
}

void logpacker::destroy()
{
	s_instance.reset();
}

std::auto_ptr<logpack> logpacker::s_instance;

