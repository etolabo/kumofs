//
// mp::shared_buffer
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

#ifndef MP_SHARED_BUFFER_H__
#define MP_SHARED_BUFFER_H__

#include <memory>
#include <stdlib.h>

#ifndef MP_SHARED_BUFFER_INITIAL_BUFFER_SIZE
#define MP_SHARED_BUFFER_INITIAL_BUFFER_SIZE 8*1024
#endif

namespace mp {


class shared_buffer {
public:
	shared_buffer(size_t init_size = MP_SHARED_BUFFER_INITIAL_BUFFER_SIZE);
	~shared_buffer();

public:
	void reserve(size_t len, size_t init_size = MP_SHARED_BUFFER_INITIAL_BUFFER_SIZE);

	void* buffer();
	size_t buffer_capacity() const;

	class reference {
	public:
		reference();
		reference(void* p);
		reference(const reference& o);
		~reference();
		void clear();
		void reset(void* p);
		void swap(reference& x);
	private:
		void* m;
	};

	void* allocate(size_t size, reference* result_ref = NULL,
			size_t init_size = MP_SHARED_BUFFER_INITIAL_BUFFER_SIZE);

private:
	char* m_buffer;
	size_t m_used;
	size_t m_free;

private:
	void expand_buffer(size_t len, size_t init_size);

	typedef volatile unsigned int count_t;
	static void init_count(void* d);
	static void decr_count(void* d);
	static void incr_count(void* d);
	static count_t get_count(void* d);

private:
	shared_buffer(const shared_buffer&);
};


}  // namespace mp

#include "mp/shared_buffer_impl.h"

#endif /* mp/shared_buffer.h */

