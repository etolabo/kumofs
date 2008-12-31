#ifndef RPC_VREFBUFFER_H__
#define RPC_VREFBUFFER_H__

#include <string.h>
#include <sys/uio.h>
#include <vector>
#include <algorithm>

namespace rpc {

// FIXME 72?
// FIXME 1024?
static const size_t VREFBUFFER_INITIAL_ALLOCATION_SIZE = 4096;

class vrefbuffer {
public:
	vrefbuffer();
	~vrefbuffer();

public:
	void write(const char* buf, size_t len);

	size_t vector_size() const;
	size_t get_vector(struct iovec* vec);

	void clear();

private:
	void expand_buffer(size_t len);
	size_t buffer_capacity() const;
	void append_ref(const char* buf, size_t len);

public:
	char* m_buffer;
	size_t m_free;

	struct entry {
		char* buffer;
		size_t used;
		bool is_ref;
	};

	typedef std::vector<entry> vec_t;
	vec_t m_vec;

private:
	vrefbuffer(const vrefbuffer&);
};


inline vrefbuffer::vrefbuffer() :
	m_buffer(NULL),
	m_free(0) { }

inline vrefbuffer::~vrefbuffer() { clear(); }


inline void vrefbuffer::write(const char* buf, size_t len)
{
	if(len > 512) {  // FIXME
		append_ref(buf, len);
	} else {
		if(m_free < len) { expand_buffer(len); }
		memcpy(m_buffer + m_vec.back().used, buf, len);
		m_vec.back().used += len;
		m_free -= len;
	}
}

inline size_t vrefbuffer::vector_size() const
{
	return m_vec.size();
}

inline size_t vrefbuffer::buffer_capacity() const
{
	return m_free;
}


}  // namespace rpc

#endif /* rpc/vrefbuffer.h */

