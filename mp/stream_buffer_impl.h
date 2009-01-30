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

#ifndef MP_STREAM_BUFFER_IMPL_H__
#define MP_STREAM_BUFFER_IMPL_H__

#include <vector>

namespace mp {


struct stream_buffer::reference {
public:
	reference() { }

	~reference()
	{
		std::for_each(m_array.begin(), m_array.end(), decl());
	}

	void push(void* d)
	{
		m_array.push_back(d);
	}

private:
	std::vector<void*> m_array;
	struct decl {
		void operator() (void* d)
		{
			stream_buffer::decl_count(d);
		}
	};
};


inline void stream_buffer::init_count(void* d)
{
	*(count_t*)d = 1;
}

inline void stream_buffer::decl_count(void* d)
{
	//if(--*(count_t*)d == 0) {
	if(__sync_sub_and_fetch((count_t*)d, 1) == 0) {
		free(d);
	}
}

inline void stream_buffer::incr_count(void* d)
{
	//++*(count_t*)d;
	__sync_add_and_fetch((count_t*)d, 1);
}

inline stream_buffer::count_t stream_buffer::get_count(void* d)
{
	return *(count_t*)d;
}


inline stream_buffer::stream_buffer(size_t initial_buffer_size) :
	m_buffer(NULL),
	m_used(0),
	m_free(0),
	m_off(0),
	m_ref(new reference()),
	m_initial_buffer_size(initial_buffer_size)
{
	if(m_initial_buffer_size < sizeof(count_t)) {
		m_initial_buffer_size = sizeof(count_t);
	}

	m_buffer = (char*)::malloc(m_initial_buffer_size);
	if(!m_buffer) { throw std::bad_alloc(); }
	init_count(m_buffer);

	m_used = sizeof(count_t);
	m_free = m_initial_buffer_size - m_used;
	m_off  = sizeof(count_t);
}

inline stream_buffer::~stream_buffer()
{
	decl_count(m_buffer);
}

inline void* stream_buffer::buffer()
{
	return m_buffer + m_used;
}

inline size_t stream_buffer::buffer_capacity() const
{
	return m_free;
}

inline void stream_buffer::buffer_consumed(size_t len)
{
	m_used += len;
	m_free -= len;
}

inline void* stream_buffer::data()
{
	return m_buffer + m_off;
}

inline size_t stream_buffer::data_size() const
{
	return m_used - m_off;
}

inline void stream_buffer::data_used(size_t len)
{
	m_off += len;
}


inline stream_buffer::reference* stream_buffer::release()
{
	// FIXME
	m_ref->push(m_buffer);
	incr_count(m_buffer);

	//std::auto_ptr<reference> old(new reference());
	//m_ref.swap(old);
	reference* n = new reference();
	std::auto_ptr<reference> old(m_ref.release());
	m_ref.reset(n);

	return old.release();
}

inline void stream_buffer::reserve_buffer(size_t len)
{
	if(m_used == m_off && get_count(m_buffer) == 1) {
		// rewind buffer
		m_free += m_used - sizeof(count_t);
		m_used = sizeof(count_t);
		m_off = sizeof(count_t);
	}
	if(m_free < len) {
		expand_buffer(len);
	}
}

inline void stream_buffer::expand_buffer(size_t len)
{
	if(m_off == sizeof(count_t)) {
		size_t next_size = (m_used + m_free) * 2;
		while(next_size < len + m_used) { next_size *= 2; }

		char* tmp = (char*)::realloc(m_buffer, next_size);
		if(!tmp) { throw std::bad_alloc(); }

		m_buffer = tmp;
		m_free = next_size - m_used;

	} else {
		size_t next_size = m_initial_buffer_size;  // include sizeof(count_t)
		size_t not_parsed = m_used - m_off;
		while(next_size < len + not_parsed + sizeof(count_t)) { next_size *= 2; }

		char* tmp = (char*)::malloc(next_size);
		if(!tmp) { throw std::bad_alloc(); }
		init_count(tmp);

		try {
			m_ref->push(m_buffer);
		} catch (...) { free(tmp); throw; }

		memcpy(tmp+sizeof(count_t), m_buffer+m_off, not_parsed);

		m_buffer = tmp;
		m_used = not_parsed + sizeof(count_t);
		m_free = next_size - m_used;
		m_off = sizeof(count_t);
	}
}


}  // namespace mp

#endif /* mp/stream_buffer_impl.h */

