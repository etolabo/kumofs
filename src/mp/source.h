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

#ifndef MP_SOURCE_H__
#define MP_SOURCE_H__

#include <cstddef>

#ifndef MP_SOURCE_DEFAULT_ALLOCATION_SIZE
#define MP_SOURCE_DEFAULT_ALLOCATION_SIZE 32*1024
#endif

#ifndef MP_SOURCE_DEFAULT_LOTS_IN_CHUNK
#define MP_SOURCE_DEFAULT_LOTS_IN_CHUNK 4
#endif

namespace mp {

static const size_t SOURCE_DEFAULT_ALLOCATION_SIZE = MP_SOURCE_DEFAULT_ALLOCATION_SIZE;
static const size_t SOURCE_DEFAULT_LOTS_IN_CHUNK = MP_SOURCE_DEFAULT_LOTS_IN_CHUNK;

template < size_t EstimatedAllocationSize = SOURCE_DEFAULT_ALLOCATION_SIZE,
	   size_t OptimalLotsInChunk = SOURCE_DEFAULT_LOTS_IN_CHUNK >
class source {
public:
	source();
	~source();

public:
	//! Allocate memory from the pool.
	/* The allocated memory have to be freed using free() function. */
	void* malloc(size_t size);

	//! Free the allocated memory.
	void free(void* x);

private:
	struct chunk_t {
		chunk_t* next;
		chunk_t* prev;
		size_t free;
		size_t lots;
		size_t size;
	};
	struct data_t {
		chunk_t* chunk;
	};

private:
	chunk_t* m_free;
	chunk_t* m_used;

private:
	void* expand_free(size_t req);
	void splice_to_used(chunk_t* chunk);
	void splice_to_free(chunk_t* chunk);

private:
	source(const source&);
};


}  // namespace mp

#include "mp/source_impl.h"

#endif /* mp/source.h */

