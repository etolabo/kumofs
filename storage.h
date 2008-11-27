#ifndef STORAGE_H__
#define STORAGE_H__

#include <stdint.h>
#include <msgpack.hpp>
#include <arpa/inet.h>

// =Database entry format
// Big endian
// +--------+--------+-----------------+
// |   64   |   64   |       ...       |
// +--------+--------+-----------------+
// clocktime
//          partial write clocktime
//                   data

namespace kumo {

#if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define __LITTLE_ENDIAN__
#elif __BYTE_ORDER == __BIG_ENDIAN
#define __BIG_ENDIAN__
#endif
#endif

#define msgpack_betoh16(x) ntohs(x)
#define msgpack_betoh32(x) ntohl(x)

#ifdef __LITTLE_ENDIAN__
#if defined(__bswap_64)
#  define Storage_beswap64(x) __bswap_64(x)
#elif defined(__DARWIN_OSSwapInt64)
#  define Storage_beswap64(x) __DARWIN_OSSwapInt64(x)
#else
static inline uint64_t Storage_beswap64(uint64_t x) {
	return	((x << 56) & 0xff00000000000000ULL ) |
			((x << 40) & 0x00ff000000000000ULL ) |
			((x << 24) & 0x0000ff0000000000ULL ) |
			((x <<  8) & 0x000000ff00000000ULL ) |
			((x >>  8) & 0x00000000ff000000ULL ) |
			((x >> 24) & 0x0000000000ff0000ULL ) |
			((x >> 40) & 0x000000000000ff00ULL ) |
			((x >> 56) & 0x00000000000000ffULL ) ;
}
#endif
#else
#define Storage_beswap64(x) (x)
#endif

class DBFormat {
public:
	DBFormat(const char* val, uint32_t vallen) :
		m_val(val), m_vallen(vallen)
	{
		if(vallen < 16) {
			throw std::runtime_error("bad data");
		}
	}

	~DBFormat() {}

	static const size_t LEADING_METADATA_SIZE = 16;

public:
	uint64_t clocktime() const
	{
		return Storage_beswap64( ((const uint64_t*)m_val)[0] );
	}

	uint64_t partialtime() const
	{
		return Storage_beswap64( ((const uint64_t*)m_val)[1] );
	}

	const char* data() const
	{
		return ((const char*)m_val) + 16;
	}

	uint32_t datalen() const
	{
		return m_vallen - 16;
	}

	msgpack::type::raw_ref raw_ref() const
	{
		return msgpack::type::raw_ref(data(), datalen());
	}

public:
	static void set_meta(char* meta_val, uint64_t clocktime, uint64_t partialtime)
	{
		((uint64_t*)meta_val)[0] = Storage_beswap64(clocktime);
		((uint64_t*)meta_val)[1] = Storage_beswap64(partialtime);
	}

private:
	const char* m_val;
	uint32_t m_vallen;
};


}  // namespace kumo

#ifdef USE_TOKYOCABINET
#include "storage/tokyocabinet.h"
#else
#include "storage/luxio.h"
#endif

#endif /* storage.h */

