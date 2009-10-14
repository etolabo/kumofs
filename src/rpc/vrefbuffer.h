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
#ifndef RPC_VREFBUFFER_H__
#define RPC_VREFBUFFER_H__

#include <msgpack.hpp>
#include <string.h>
#include <sys/uio.h>
#include <vector>
#include <algorithm>

#ifndef VREFBUFFER_REF_SIZE
#define VREFBUFFER_REF_SIZE 32  // FIXME
#endif

namespace rpc {


class vrefbuffer {
public:
	vrefbuffer();
	~vrefbuffer();

public:
	void append_ref(const char* buf, size_t len);
	void append_copy(const char* buf, size_t len);

	void write(const char* buf, size_t len);

	size_t vector_size() const;
	const struct iovec* vector() const;

public:
	typedef std::vector<struct iovec> vec_t;
	vec_t m_vec;

	msgpack::zone m_zone;

private:
	vrefbuffer(const vrefbuffer&);
};


inline vrefbuffer::vrefbuffer()
{
	m_vec.reserve(4);  // FIXME sizeof(struct iovec) * 4 < 72
}

inline vrefbuffer::~vrefbuffer() { }


inline void vrefbuffer::append_ref(const char* buf, size_t len)
{
	struct iovec v = {(void*)buf, len};
	m_vec.push_back(v);
}

inline void vrefbuffer::append_copy(const char* buf, size_t len)
{
	char* m = (char*)m_zone.malloc(len);
	memcpy(m, buf, len);
	if(!m_vec.empty() && ((const char*)m_vec.back().iov_base) +
			m_vec.back().iov_len == m) {
		m_vec.back().iov_len += len;
	} else {
		append_ref(m, len);
	}
}


inline void vrefbuffer::write(const char* buf, size_t len)
{
	if(len > VREFBUFFER_REF_SIZE) {
		append_ref(buf, len);
	} else {
		append_copy(buf, len);
	}
}


inline size_t vrefbuffer::vector_size() const
{
	return m_vec.size();
}

inline const struct iovec* vrefbuffer::vector() const
{
	return &m_vec[0];
}


}  // namespace rpc

#endif /* rpc/vrefbuffer.h */

