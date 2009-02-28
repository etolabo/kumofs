#ifndef RPC_SBUFFER_H__
#define RPC_SBUFFER_H__

#include <string.h>
#include <stdlib.h>
#include <stdexcept>

namespace rpc {


class sbuffer {
public:
	static const size_t INITIAL_ALLOCATION_SIZE = 1024;

	sbuffer() :
		m_free(INITIAL_ALLOCATION_SIZE),
		m_used(0),
		m_storage((char*)::malloc(INITIAL_ALLOCATION_SIZE))
	{
		if(!m_storage) { throw std::bad_alloc(); }
	}

	~sbuffer()
	{
		free(m_storage);
	}

public:
	void write(const char* buf, size_t len)
	{
		if(m_free < len) {
			expand_buffer(len);
		}
		memcpy(m_storage + m_used, buf, len);
		m_used += len;
		m_free -= len;
	}

	void* data()
	{
		return m_storage;
	}

	size_t size() const
	{
		return m_used;
	}

	void release()
	{
		m_storage = NULL;
		m_free = 0;
		m_used = 0;
	}

private:
	void expand_buffer(size_t req)
	{
		size_t nsize;
		if(!m_storage) {
			nsize = INITIAL_ALLOCATION_SIZE;
		} else {
			nsize = (m_free + m_used) * 2;
		}

		while(nsize < m_used + req) { nsize *= 2; }

		char* tmp = (char*)realloc(m_storage, nsize);
		if(!tmp) {
			throw std::bad_alloc();
		}

		m_storage = tmp;
		m_free = nsize - m_used;
	}

private:
	size_t m_free;
	size_t m_used;
	char* m_storage;

private:
	sbuffer(const sbuffer&);
};


}  // namespace rpc

#endif /* rpc/sbuffer.h */

