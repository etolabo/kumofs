#ifndef KUMO_STORAGE_H__
#define KUMO_STORAGE_H__

#include <msgpack.h>
#include <stddef.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>

/* Big endian
 *
 * key:
 * +--------+-----------------+
 * |   64   |       ...       |
 * +--------+-----------------+
 * hash
 *          key
 *
 * value:
 * +--------+--------+-----------------+
 * |   64   |   64   |       ...       |
 * +--------+--------+-----------------+
 * clocktime
 *          meta
 *                   data
 */

#ifdef __cplusplus
extern "C" {
#endif

static uint64_t kumo_storage_clocktime_of(const char* val);
static void kumo_storage_clocktime_to(uint64_t clocktime, char* buf8bytes);

static uint64_t kumo_storage_hash_of(const char* key);
static void kumo_storage_hash_to(uint64_t hash, char* buf8bytes);

static bool kumo_clocktime_less(uint64_t is_this_one, uint64_t older_than_this_one);

void kumo_start_timer(const struct timespec* interval,
		void (*func)(void* data), void* data);


typedef struct {

	// failed: NULL
	void* (*create)(void);

	void (*free)(void* data);

	// success: NULL;  faied: message
	const char* (*open)(void* data, int* argc, char** argv);

	void (*close)(void* data);

	// found: value;  not-found: NULL
	const char* (*get)(void* data,
			const char* key, uint32_t keylen,
			uint32_t* result_vallen,
			msgpack_zone* zone);

	// success: true;  failed: false
	bool (*set)(void* data,
			const char* key, uint32_t keylen,
			const char* val, uint32_t vallen);

	// updated: true;  not-updated: false
	bool (*update)(void* data,
			const char* key, uint32_t keylen,
			const char* val, uint32_t vallen);

	// number of processed keys
	int (*updatev)(void* data,
			const char** keys, const size_t* keylens,
			const char** vals, const size_t* vallens,
			uint16_t num);

	// deleted: true;  not-deleted: false
	bool (*del)(void* data,
			const char* key, uint32_t keylen,
			uint64_t clocktime);

	uint64_t (*rnum)(void* data);

	// success: true;  not-success: false
	bool (*backup)(void* data, const char* dstpath);

	// success >= 0;  failed < 0
	int (*for_each)(void* data,
			void* user,
			int (*func)(void* user, void* iterator_data));

	const char* (*iterator_key)(void* iterator_data);
	const char* (*iterator_val)(void* iterator_data);

	size_t (*iterator_vallen)(void* iterator_data);
	size_t (*iterator_keylen)(void* iterator_data);

	// success: true;  failed: false
	bool (*iterator_release_key)(void* iterator_data, msgpack_zone* zone);
	bool (*iterator_release_val)(void* iterator_data, msgpack_zone* zone);

	// deleted: true;  not-deleted: false
	bool (*iterator_delete)(void* iterator_data);
	bool (*iterator_delete_if_older)(void* iterator_data, uint64_t if_older);

} kumo_storage_op;


kumo_storage_op kumo_storage_init();



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

inline uint64_t kumo_storage_clocktime_of(const char* val)
{
	return kumo_be64(*(uint64_t*)val);
}

inline void kumo_storage_clocktime_to(uint64_t clocktime, char* buf8bytes)
{
	*(uint64_t*)buf8bytes = kumo_be64(clocktime);
}

inline uint64_t kumo_storage_hash_of(const char* key)
{
	return kumo_be64(*(uint64_t*)key);
}

inline void kumo_storage_hash_to(uint64_t hash, char* buf8bytes)
{
	*(uint64_t*)buf8bytes = kumo_be64(hash);
}

inline bool kumo_clocktime_less(uint64_t x, uint64_t y)
{
	if((x < (((uint32_t)1)<<10) && (((uint32_t)1)<<22) < y) ||
	   (y < (((uint32_t)1)<<10) && (((uint32_t)1)<<22) < x)) {
		return x > y;
	} else {
		return x < y;
	}
}


#ifdef __cplusplus
}
#endif

#endif /* kumo/storage.h */

