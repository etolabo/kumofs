//
// mpio utility
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

#ifndef MP_UTILITY_H__
#define MP_UTILITY_H__

#include "mp/exception.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

namespace mp {


template <unsigned int ThreadID>
struct thread_tag {
	static const unsigned int ID = ThreadID;
};

typedef thread_tag<0> main_thread_tag;


inline void set_nonblock(int fd)
{
	if( ::fcntl(fd, F_SETFL, O_NONBLOCK) < 0 ) {
		throw system_error(errno, "failed to set nonblock flag");
	}
}


}  // namespace mp

#endif /* mp/utility.h */

