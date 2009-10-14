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
#ifndef BUFFER_QUEUE_H__
#define BUFFER_QUEUE_H__

#include <queue>
#include <stdlib.h>
#include <string.h>
#include <mp/source.h>

namespace kumo {


class buffer_queue {
public:
	buffer_queue();
	~buffer_queue();

public:
	void push(const void* buf, size_t buflen);
	const void* front(size_t* result_buflen) const;
	void pop();

	size_t total_size() const;

private:
	size_t m_total_size;

	struct entry {
		void* data;
		size_t size;
	};

	typedef std::queue<entry> queue_type;
	queue_type m_queue;

	mp::source<128, 2048> m_source;
};

inline buffer_queue::buffer_queue() :
	m_total_size(0) { }

inline buffer_queue::~buffer_queue()
{
	// source::~source frees all memory
	//for(queue_type::iterator it(m_queue.begin()),
	//		it_end(m_queue.end()); it != it_end; ++it) {
	//	m_source.free(*it);
	//}
}

inline void buffer_queue::push(const void* buf, size_t buflen)
{
	void* data = m_source.malloc(buflen);
	::memcpy(data, buf, buflen);

	entry e = {data, buflen};
	try {
		m_queue.push(e);
	} catch (...) {
		m_source.free(data);
		throw;
	}

	m_total_size += buflen;
}

inline const void* buffer_queue::front(size_t* result_buflen) const
{
	if(m_queue.empty()) {
		return NULL;
	}

	const entry& e = m_queue.front();

	*result_buflen = e.size;
	return e.data;
}

inline void buffer_queue::pop()
{
	entry& e = m_queue.front();

	m_total_size -= e.size;
	m_source.free( e.data );

	m_queue.pop();
}

inline size_t buffer_queue::total_size() const
{
	return m_total_size;
}


}  // namespace kumo


#if 0
using kumo::buffer_queue;

struct kumo_buffer_queue;

kumo_buffer_queue* kumo_buffer_queue_new(void)
try {
	buffer_queue* impl = new buffer_queue();
	return reinterpret_cast<kumo_buffer_queue*>(impl);
} catch (...) {
	return NULL;
}

void kumo_buffer_queue_free(kumo_buffer_queue* bq)
try {
	buffer_queue* impl = reinterpret_cast<buffer_queue*>(bq);
	delete impl;
} catch (...) { }

bool kumo_buffer_queue_push(kumo_buffer_queue* bq, const void* buf, size_t buflen)
try {
	buffer_queue* impl = reinterpret_cast<buffer_queue*>(bq);
	impl->push(buf, buflen);
	return true;
} catch (...) {
	return false;
}

const void* kumo_buffer_queue_front(kumo_buffer_queue* bq, size_t* result_buflen)
{
	buffer_queue* impl = reinterpret_cast<buffer_queue*>(bq);
	return impl->front(result_buflen);
}

void kumo_buffer_queue_pop(kumo_buffer_queue* bq)
try {
	buffer_queue* impl = reinterpret_cast<buffer_queue*>(bq);
	impl->pop();
} catch (...) { }

size_t kumo_buffer_queue_total_size(kumo_buffer_queue* bq)
{
	buffer_queue* impl = reinterpret_cast<buffer_queue*>(bq);
	return impl->total_size();
}
#endif


#endif /* storage/buffer_queue.h */

