#ifndef LOGPACKER_H__
#define LOGPACKER_H__

#include "logpack.hpp"

class logpacker {
public:
	static void initialize(const std::string& basename, size_t lotate_size)
	{
		s_instance.reset(new logpack(basename, lotate_size));
	}

	static void destroy() { s_instance.reset(); }

	bool is_active() { return s_instance; }
	logpack& instance() { return *s_instance; }

private:
	static std::auto_ptr<logpack> s_instance;
};

#define LOGPACK(name, version, ...) \
	do { \
		if(logpacker::is_active()) { \
			logpacker::instance().write(name, version, __VA_ARGS__); \
		} \
	} while(0)

#endif /* logpacker.h */

