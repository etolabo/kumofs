#ifndef STORAGE_H__
#define STORAGE_H__

#include <stdint.h>
#include <msgpack.hpp>
#include <arpa/inet.h>

// =Database entry format
// Big endian
//
// key:
// +--------+-----------------+
// |   64   |       ...       |
// +--------+-----------------+
// hash
//          key
//
// value:
// +--------+--------+-----------------+
// |   64   |   64   |       ...       |
// +--------+--------+-----------------+
// clocktime
//          meta
//                   data

#ifdef __LITTLE_ENDIAN__
#if defined(__bswap_64)
#  define kumo_be64(x) __bswap_64(x)
#elif defined(__DARWIN_OSSwapInt64)
#  define kumo_be64(x) __DARWIN_OSSwapInt64(x)
#else
static inline uint64_t kumo_be64(uint64_t x) {
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
#define kumo_be64(x) (x)
#endif

namespace kumo {


class Storage;

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

	static const size_t VALUE_META_SIZE = 16;
	static const size_t KEY_META_SIZE = 8;

	static bool get_clocktime(Storage& db,
			const char* key, size_t keylen, uint64_t* result);

	static uint64_t clocktime(const char* val)
	{
		return kumo_be64(*(uint64_t*)val);
	}

	static uint64_t meta(const char* val)
	{
		return kumo_be64(*(uint64_t*)(val+8));
	}

	static uint64_t hash(const char* key)
	{
		return kumo_be64(*(uint64_t*)key);
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

namespace kumo {
	inline bool DBFormat::get_clocktime(Storage& db,
			const char* key, size_t keylen, uint64_t* result)
	{
		char buf[8];
		int32_t len = db.get_header(key, keylen, buf, sizeof(buf));
		if(len < (int32_t)sizeof(buf)) { return false; }
		*result = kumo_be64(*(uint64_t*)buf);
		return true;
	}
}  // namespace kumo

#endif /* storage.h */

