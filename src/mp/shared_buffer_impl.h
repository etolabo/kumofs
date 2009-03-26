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

#ifndef MP_SHARED_BUFFER_IMPL_H__
#define MP_SHARED_BUFFER_IMPL_H__

namespace mp {


inline void shared_buffer::init_count(void* d)
{
	*(volatile count_t*)d = 1;
}

inline void shared_buffer::decr_count(void* d)
{
	//if(--*(count_t*)d == 0) {
	if(__sync_sub_and_fetch((count_t*)d, 1) == 0) {
		::free(d);
	}
}

inline void shared_buffer::incr_count(void* d)
{
	//++*(count_t*)d;
	__sync_add_and_fetch((count_t*)d, 1);
}

inline shared_buffer::count_t shared_buffer::get_count(void* d)
{
	return *(count_t*)d;
}


inline shared_buffer::reference::reference() : m(NULL) { }

inline shared_buffer::reference::reference(void* p) :
	m(p)
{
	incr_count(m);
}

inline shared_buffer::reference::reference(const reference& o) :
	m(o.m)
{
	incr_count(m);
}

inline void shared_buffer::reference::clear()
{
	if(m) {
		decr_count(m);
		m = NULL;
	}
}

inline shared_buffer::reference::~reference()
{
	clear();
}

inline void shared_buffer::reference::reset(void* p)
{
	clear();
	m = p;
	incr_count(m);
}

inline void shared_buffer::reference::swap(reference& x)
{
	void* tmp = m;
	m = x.m;
	x.m = tmp;
}


inline shared_buffer::shared_buffer(size_t init_size)
{
	const size_t initsz = std::max(init_size, sizeof(count_t));
	m_buffer = (char*)::malloc(initsz);
	if(m_buffer == NULL) { throw std::bad_alloc(); }

	init_count(m_buffer);
	m_used = sizeof(count_t);
	m_free = initsz - m_used;
}

inline shared_buffer::~shared_buffer()
{
	decr_count(m_buffer);
}

inline void* shared_buffer::buffer()
{
	return m_buffer + m_used;
}

inline size_t shared_buffer::buffer_capacity() const
{
	return m_free;
}

inline void shared_buffer::reserve(size_t len, size_t init_size)
{
	if(get_count(m_buffer) == 1) {
		// rewind buffer
		m_free += m_used - sizeof(count_t);
		m_used = sizeof(count_t);
	}
	if(m_free < len) {
		expand_buffer(len, init_size);
	}
}

inline void* shared_buffer::allocate(size_t len,
		reference* result_ref, size_t init_size)
{
	reserve(len, init_size);
	char* tmp = m_buffer + m_used;
	m_used += len;
	m_free -= len;
	if(result_ref) {
		result_ref->reset(m_buffer);
	}
	return tmp;
}

inline void shared_buffer::expand_buffer(size_t len, size_t init_size)
{
	if(m_used == sizeof(count_t)) {
		size_t next_size = (m_used + m_free) * 2;
		while(next_size < len + m_used) { next_size *= 2; }

		char* tmp = (char*)::realloc(m_buffer, next_size);
		if(!tmp) { throw std::bad_alloc(); }

		m_buffer = tmp;
		m_free = next_size - m_used;

	} else {
		const size_t initsz = std::max(init_size, sizeof(count_t));

		size_t next_size = initsz;  // include sizeof(count_t)
		while(next_size < len + sizeof(count_t)) { next_size *= 2; }

		char* tmp = (char*)::malloc(next_size);
		if(!tmp) { throw std::bad_alloc(); }
		init_count(tmp);

		decr_count(m_buffer);

		m_buffer = tmp;
		m_used = sizeof(count_t);
		m_free = next_size - m_used;
	}
}


}  // namespace mp

#endif /* mp/shared_buffer_impl.h */

