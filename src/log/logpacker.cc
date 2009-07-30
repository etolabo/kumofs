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

