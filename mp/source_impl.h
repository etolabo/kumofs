//
// mp::source
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

#ifndef MP_SOURCE_IMPL_H__
#define MP_SOURCE_IMPL_H__

#include <cstdlib>
#include <stdexcept>

namespace mp {


template <size_t EstimatedAllocationSize, size_t OptimalLotsInChunk>
source<EstimatedAllocationSize, OptimalLotsInChunk>::source()
{
	m_free = (chunk_t*)std::malloc(sizeof(chunk_t));
	if( m_free == NULL ) { throw std::bad_alloc(); }
	m_used = (chunk_t*)std::malloc(sizeof(chunk_t));
	if( m_used == NULL ) { std::free(m_free); throw std::bad_alloc(); }
	m_free->next = m_free;
	m_free->prev = m_free;
	m_used->next = m_used;
	m_used->prev = m_used;
}

template <size_t EstimatedAllocationSize, size_t OptimalLotsInChunk>
source<EstimatedAllocationSize, OptimalLotsInChunk>::~source()
{
	chunk_t* f = m_free->next;
	while(f != m_free) {
		f = f->next;
		std::free(f->prev);
	}
	std::free(f);

	f = m_used;
	while(f->next != m_used) {
		f = f->next;
		std::free(f->prev);
	}
	std::free(f);
}

template <size_t EstimatedAllocationSize, size_t OptimalLotsInChunk>
void* source<EstimatedAllocationSize, OptimalLotsInChunk>::malloc(size_t size)
{
	size_t req = size + sizeof(data_t);
	for( chunk_t* f = m_free->next; f != m_free; f = f->next ) {
		if( f->free < req ) { continue; }
		data_t* data = reinterpret_cast<data_t*>(
				((char*)f) + sizeof(chunk_t) + f->size - f->free
				);
		f->lots++;
		f->free -= req;
		data->chunk = f;
		if( f->free < EstimatedAllocationSize + sizeof(chunk_t) ) {
			splice_to_used(f);
		}
		return ((char*)data) + sizeof(data_t);
	}
	return expand_free(req);
}

template <size_t EstimatedAllocationSize, size_t OptimalLotsInChunk>
void source<EstimatedAllocationSize, OptimalLotsInChunk>::free(void* x)
{
	data_t* data = reinterpret_cast<data_t*>( ((char*)x) - sizeof(data_t) );
	chunk_t* chunk = data->chunk;
	chunk->lots--;
	if( chunk->lots == 0 ) {
		splice_to_free(chunk);
	}
}

template <size_t EstimatedAllocationSize, size_t OptimalLotsInChunk>
void* source<EstimatedAllocationSize, OptimalLotsInChunk>::expand_free(size_t req)
{
	const size_t default_chunk_size = (EstimatedAllocationSize + sizeof(chunk_t)) * OptimalLotsInChunk;
	size_t chunk_size = req > default_chunk_size ? req : default_chunk_size;
	chunk_t* n = (chunk_t*)std::malloc(sizeof(chunk_t) + chunk_size);
	if( n == NULL ) { throw std::bad_alloc(); }
	data_t* data = reinterpret_cast<data_t*>( ((char*)n) + sizeof(chunk_t) );
	n->lots = 1;
	n->size = chunk_size;
	n->free = chunk_size - req;
	data->chunk = n;
	if( n->free < EstimatedAllocationSize + sizeof(chunk_t) ) {
		n->prev = m_used;
		n->next = m_used->next;
		m_used->next->prev = n;
		m_used->next = n;
	} else {
		n->prev = m_free;
		n->next = m_free->next;
		m_free->next->prev = n;
		m_free->next = n;
	}
	return ((char*)data) + sizeof(data_t);
}

template <size_t EstimatedAllocationSize, size_t OptimalLotsInChunk>
void source<EstimatedAllocationSize, OptimalLotsInChunk>::splice_to_used(chunk_t* chunk)
{
	chunk->prev->next = chunk->next;
	chunk->next->prev = chunk->prev;
	chunk->prev = m_used;
	chunk->next = m_used->next;
	m_used->next->prev = chunk;
	m_used->next = chunk;
}

template <size_t EstimatedAllocationSize, size_t OptimalLotsInChunk>
void source<EstimatedAllocationSize, OptimalLotsInChunk>::splice_to_free(chunk_t* chunk)
{
	chunk->prev->next = chunk->next;
	chunk->next->prev = chunk->prev;
	chunk->next = m_free->next;
	chunk->prev = m_free;
	m_free->next->prev = chunk;
	m_free->next = chunk;
	chunk->free = chunk->size;
}


}  // namespace mp

#endif /* mp/source_impl.h */

