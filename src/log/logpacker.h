#ifndef LOGPACKER_H__
#define LOGPACKER_H__

#include "logpack.hpp"
#include <memory>

class logpacker {
public:
	static void initialize(const char* fname);
	static void destroy();
	static void reopen() { s_instance->reopen(); }
	static bool is_active() { return !!s_instance.get(); }
	static logpack& instance() { return *s_instance; }
private:
	static std::auto_ptr<logpack> s_instance;
};

#define LOGPACK(name, version, ...) \
	do { \
		if(logpacker::is_active()) { \
			logpacker::instance().write(name, version, __VA_ARGS__); \
		} \
	} while(0)

#define TLOGPACK(name, version, ...) \
	LOGPACK(name, version, "time", time(NULL), __VA_ARGS__)

#endif /* logpacker.h */

