//
// kumofs
//
// Copyright (C) 2009 Etolabo Corp.
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
