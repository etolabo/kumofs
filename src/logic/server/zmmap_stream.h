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
#ifndef SERVER_ZMMAP_STREAM_H__
#define SERVER_ZMMAP_STREAM_H__

#include <zlib.h>
#include <mp/exception.h>

#ifndef ZMMAP_STREAM_INITIAL_SIZE
#define ZMMAP_STREAM_INITIAL_SIZE (1024*1024)
#endif

#ifndef ZMMAP_STREAM_RESERVE_SIZE
#define ZMMAP_STREAM_RESERVE_SIZE (8*1024)
#endif

namespace kumo {
namespace server {


class zmmap_stream {
public:
	zmmap_stream(int fd);
	~zmmap_stream();
	size_t size() const;

	void write(const void* buf, size_t len);
	void flush();

private:
	z_stream m_z;

	char* m_map;
	int m_fd;
	void expand_map(size_t req);

private:
	zmmap_stream();
	zmmap_stream(const zmmap_stream&);
};


inline size_t zmmap_stream::size() const
{
	return (char*)m_z.next_out - m_map;
}


inline void zmmap_stream::write(const void* buf, size_t len)
{
	m_z.next_in = (Bytef*)buf;
	m_z.avail_in = len;

	do {
		if(m_z.avail_out < ZMMAP_STREAM_RESERVE_SIZE) {
			expand_map(ZMMAP_STREAM_INITIAL_SIZE);
		}

		if(deflate(&m_z, Z_NO_FLUSH) != Z_OK) {
			throw std::runtime_error("deflate failed");
		}

	} while(m_z.avail_in > 0);
}


}  // namespace server
}  // namespace kumo

#endif  /* server/zmmap_stream.h */

