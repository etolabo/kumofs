#ifndef KUMO_STORAGE_H__
#define KUMO_STORAGE_H__

#include <msgpack.h>
#include <stddef.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef bool (*kumo_storage_casproc)(void* casdata, const char* oldval, size_t oldvallen);

typedef struct {

	// failed: NULL
	void* (*create)(void);

	void (*free)(void* data);

	// success: true;  faied: false
	//bool (*open)(void* data, int* argc, char** argv);
	bool (*open)(void* data, const char* path);

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

	// deleted: true;  not-deleted: false
	bool (*del)(void* data,
			const char* key, uint32_t keylen,
			kumo_storage_casproc proc, void* casdata);

	// updated: true;  not-updated: false
	bool (*update)(void* data,
			const char* key, uint32_t keylen,
			const char* val, uint32_t vallen,
			kumo_storage_casproc proc, void* casdata);

	// number of processed keys
	int (*updatev)(void* data,
			const char** keys, const size_t* keylens,
			const char** vals, const size_t* vallens,
			uint16_t num);

	// number of stored keys
	uint64_t (*rnum)(void* data);

	// success: true;  not-success: false
	bool (*backup)(void* data, const char* dstpath);

	const char* (*error)(void* data);

	// success >= 0;  failed < 0
	int (*for_each)(void* data,
			void* user,
			int (*func)(void* user, void* iterator_data));

	const char* (*iterator_key)(void* iterator_data);
	const char* (*iterator_val)(void* iterator_data);

	size_t (*iterator_keylen)(void* iterator_data);
	size_t (*iterator_vallen)(void* iterator_data);

	// success: released pointer;  failed: NULL
	const char* (*iterator_release_key)(void* iterator_data, msgpack_zone* zone);
	const char* (*iterator_release_val)(void* iterator_data, msgpack_zone* zone);

	// deleted: true;  not-deleted: false
	bool (*iterator_del)(void* iterator_data,
			kumo_storage_casproc proc, void* casdata);

	// deleted: true;  not-deleted: false
	bool (*iterator_del_force)(void* iterator_data);

} kumo_storage_op;


kumo_storage_op kumo_storage_init(void);


#ifdef __cplusplus
}
#endif

#endif /* kumo/storage.h */

