#include "server/zmmap_stream.h"
#include <sys/mman.h>

namespace kumo {
namespace server {


zmmap_stream::zmmap_stream(int fd) :
	m_fd(fd)
{
	m_z.zalloc = Z_NULL;
	m_z.zfree = Z_NULL;
	m_z.opaque = Z_NULL;
	if(deflateInit(&m_z, Z_DEFAULT_COMPRESSION) != Z_OK) {
		throw std::runtime_error(m_z.msg);
	}

	if(::ftruncate(m_fd, ZMMAP_STREAM_INITIAL_SIZE) < 0) {
		deflateEnd(&m_z);
		throw mp::system_error(errno, "failed to truncate offer storage");
	}

	m_map = (char*)::mmap(NULL, ZMMAP_STREAM_INITIAL_SIZE,
			PROT_WRITE, MAP_SHARED, m_fd, 0);
	if(m_map == MAP_FAILED) {
		deflateEnd(&m_z);
		throw mp::system_error(errno, "failed to mmap offer storage");
	}

	m_z.avail_out = ZMMAP_STREAM_INITIAL_SIZE;
	m_z.next_out = (Bytef*)m_map;
}

zmmap_stream::~zmmap_stream()
{
	size_t used = (char*)m_z.next_out - m_map;
	size_t csize = used + m_z.avail_out;
	::munmap(m_map, csize);
	//::ftruncate(m_fd, used);
	deflateEnd(&m_z);
}

void zmmap_stream::flush()
{
	while(true) {
		switch(deflate(&m_z, Z_FINISH)) {

		case Z_STREAM_END:
			return;

		case Z_OK:
			break;

		default:
			throw std::runtime_error("deflate flush failed");
		}

		expand_map(m_z.avail_in);
	}
}

void zmmap_stream::expand_map(size_t req)
{
	size_t used = (char*)m_z.next_out - m_map;
	size_t csize = used + m_z.avail_out;
	size_t nsize = csize * 2;
	while(nsize < req) { nsize *= 2; }

	if(::ftruncate(m_fd, nsize) < 0 ) {
		throw mp::system_error(errno, "failed to resize offer storage");
	}

#ifdef __linux__
	void* tmp = ::mremap(m_map, csize, nsize, MREMAP_MAYMOVE);
	if(tmp == MAP_FAILED) {
		throw mp::system_error(errno, "failed to mremap offer storage");
	}
	m_map = (char*)tmp;

#else
	if(::munmap(m_map, csize) < 0) {
		throw mp::system_error(errno, "failed to munmap offer storage");
	}
	m_map = NULL;
	m_z.next_out = NULL;
	m_z.avail_out = 0;

	m_map = (char*)::mmap(NULL, nsize,
			PROT_WRITE, MAP_SHARED, m_fd, 0);
	if(m_map == MAP_FAILED) {
		throw mp::system_error(errno, "failed to mmap");
	}

#endif
	m_z.next_out = (Bytef*)(m_map + used);
	m_z.avail_out = nsize - used;
}


}  // namespace server
}  // namespace kumo

