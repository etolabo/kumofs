//
// MessagePack fast log format
//
// Copyright (C) 2008-2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
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

