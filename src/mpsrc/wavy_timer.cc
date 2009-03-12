//
// mp::wavy::core::timer
//
// Copyright (C) 2008 FURUHASHI Sadayuki
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

#include "wavy_core.h"
#include <time.h>

namespace mp {
namespace wavy {


class core::impl::timer_thread {
public:
	timer_thread(core* c,
			const timespec* interval,
			timer_callback_t callback) :
		m_interval(*interval),
		m_core(c), m_callback(callback) { }

	void operator() ()
	{
		while(!m_core->is_end()) {
			nanosleep(&m_interval, NULL);
			m_core->submit(m_callback);
		}
	}

private:
	const timespec m_interval;
	core* m_core;
	timer_callback_t m_callback;
	timer_thread();
};

void core::timer(const timespec* interval, timer_callback_t callback)
{
	add_thread(1);
	impl::timer_thread t(this, interval, callback);
	submit(t);
}


}  // namespace wavy
}  // namespace mp

