//
// mp::stream_buffer
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

#ifndef MP_STREAM_BUFFER_H__
#define MP_STREAM_BUFFER_H__

#include <memory>
#include <stdlib.h>
#include <vector>
#include <algorithm>

#ifndef MP_STREAM_BUFFER_INITIAL_BUFFER_SIZE
#define MP_STREAM_BUFFER_INITIAL_BUFFER_SIZE 8*1024
#endif

namespace mp {


class stream_buffer {
public:
	stream_buffer(size_t initial_buffer_size = MP_STREAM_BUFFER_INITIAL_BUFFER_SIZE);
	~stream_buffer();

public:
	void reserve_buffer(size_t len, size_t initial_buffer_size = MP_STREAM_BUFFER_INITIAL_BUFFER_SIZE);

	void* buffer();
	size_t buffer_capacity() const;
	void buffer_consumed(size_t len);

	void* data();
	size_t data_size() const;
	void data_used(size_t len);

	class reference {
	public:
		reference();
		reference(const reference& o);
		~reference();
		void clear();
		void push(void* d);
		void swap(reference& x);
	private:
		std::vector<void*> m_array;
		struct each_incr;
		struct each_decr;
	};

	reference* release();
	void release_to(reference* to);

private:
	char* m_buffer;
	size_t m_used;
	size_t m_free;
	size_t m_off;
	reference m_ref;

private:
	void expand_buffer(size_t len, size_t initial_buffer_size);

	typedef volatile unsigned int count_t;
	static void init_count(void* d);
	static void decr_count(void* d);
	static void incr_count(void* d);
	static count_t get_count(void* d);

private:
	stream_buffer(const stream_buffer&);
};


}  // namespace mp

#include "mp/stream_buffer_impl.h"

#endif /* mp/stream_buffer.h */

