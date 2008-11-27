#include "rpc/vrefbuffer.h"

namespace rpc {


size_t vrefbuffer::get_vector(struct iovec* vec)
{
	size_t i = 0;
	for(vec_t::iterator it(m_vec.begin()), it_end(m_vec.end());
			it != it_end; ++it) {
		if(it->used > 0) {
			vec[i].iov_base = (void*)it->buffer;
			vec[i].iov_len = it->used;
			++i;
		}
	}
	return i;
}

void vrefbuffer::expand_buffer(size_t len)
{
	if(m_buffer && m_vec.back().used == 0) {
		size_t nlen = VREFBUFFER_INITIAL_ALLOCATION_SIZE*2;
		while(nlen < len) { nlen *= 2; }
		char* tmp = (char*)::realloc(m_buffer, len);
		if(!tmp) { throw std::bad_alloc(); }
		m_buffer = m_vec.back().buffer = tmp;
		m_free = len - m_vec.back().used;

	} else {
		len = std::max(len, VREFBUFFER_INITIAL_ALLOCATION_SIZE);
		char* buf = (char*)::malloc(len);
		if(!buf) { throw std::bad_alloc(); }
		try {
			entry v = { buf, 0, false };
			m_vec.push_back(v);
		} catch (...) {
			free(buf);
			throw;
		}
		m_buffer = buf;
		m_free = len;
	}
}

void vrefbuffer::append_ref(const char* buf, size_t len)
{
	entry ref = { (char*)buf, len, true };
	if(m_buffer == NULL) {
		m_vec.push_back(ref);

	} else if(m_vec.back().used == 0) {
		entry empty = m_vec.back();
		m_vec.back() = ref;
		try {
			m_vec.push_back(empty);
		} catch (...) {
			m_vec.back() = empty;
			throw;
		}

	} else {
		m_vec.push_back(ref);
		m_buffer = NULL;
		m_free = 0;
	}
}

void vrefbuffer::clear()
{
	for(vec_t::iterator it(m_vec.begin()), it_end(m_vec.end());
			it != it_end; ++it) {
		if(!it->is_ref) {
			free(it->buffer);
		}
	}
	m_vec.clear();
	m_buffer = NULL;
	m_free = 0;
}


}  // namespace rpc

