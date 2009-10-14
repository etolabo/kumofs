//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
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
#ifndef LOGIC_CLOCK_LOGIC__
#define LOGIC_CLOCK_LOGIC__

#include "logic/clock.h"

namespace kumo {


class clock_logic {
public:
	clock_logic()
	{
		clock_update_time();
	}

	~clock_logic() { }

	// interrupt rpc_server::timer_handler and call me regularly.
	void clock_update_time()
	{
		m_time = time(NULL);
	}

public:
	void clock_update(Clock c)
	{
		m_clock.update(c.get());
	}

	ClockTime clocktime_now() const
	{
		return ClockTime(m_clock.get(), m_time);
	}

	Clock clock_incr()
	{
		return Clock( m_clock.get_incr() );
	}

	ClockTime clock_incr_clocktime()
	{
		return ClockTime(m_clock.get_incr(), m_time);
	}

	uint32_t clock_get_time() const
	{
		return m_time;
	}

private:
	Clock m_clock;
	uint32_t m_time;
};


}  // namespace kumo

#endif /* logic/clock_logic.h */

