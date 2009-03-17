#include "storage/interface.h"  // FIXME
#include <tcadb.h>
#include <tcutil.h>
#include <mp/pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BACKUP_TMP_SUFFIX ".tmp"

struct kumo_tcadb {
	kumo_tcadb()
	{
		db = tcadbnew();
		if(!db) {
			throw std::bad_alloc();
		}
	}

	~kumo_tcadb()
	{
		tcadbdel(db);
	}

	TCADB* db;
	mp::pthread_mutex iterator_mutex;

private:
	kumo_tcadb(const kumo_tcadb&);
};


static void* kumo_tcadb_create(void)
try {
	kumo_tcadb* ctx = new kumo_tcadb();
	return reinterpret_cast<void*>(ctx);

} catch (...) {
	return NULL;
}

static void kumo_tcadb_free(void* data)
{
	kumo_tcadb* ctx = reinterpret_cast<kumo_tcadb*>(data);
	delete ctx;
}

static bool kumo_tcadb_open(void* data, const char* path)
{
	kumo_tcadb* ctx = reinterpret_cast<kumo_tcadb*>(data);

	if(!tcadbopen(ctx->db, path)) {
		return false;
	}

	return true;
}

static void kumo_tcadb_close(void* data)
{
	kumo_tcadb* ctx = reinterpret_cast<kumo_tcadb*>(data);
	tcadbclose(ctx->db);
}


static const char* kumo_tcadb_get(void* data,
		const char* key, uint32_t keylen,
		uint32_t* result_vallen,
		msgpack_zone* zone)
{
	kumo_tcadb* ctx = reinterpret_cast<kumo_tcadb*>(data);

	int len;
	char* val = (char*)tcadbget(ctx->db, key, keylen, &len);
	if(!val) {
		return NULL;
	}
	*result_vallen = len;

	if(!msgpack_zone_push_finalizer(zone, free, val)) {
		free(val);
		return NULL;
	}

	return val;
}

static bool kumo_tcadb_set(void* data,
		const char* key, uint32_t keylen,
		const char* val, uint32_t vallen)
{
	kumo_tcadb* ctx = reinterpret_cast<kumo_tcadb*>(data);
	return tcadbput(ctx->db, key, keylen, val, vallen);
}


typedef struct {
	bool deleted;
	kumo_storage_casproc proc;
	void* casdata;
} kumo_tcadb_del_ctx;

static void* kumo_tcadb_del_proc(const void* vbuf, int vsiz, int* sp, void* op)
{
	kumo_tcadb_del_ctx* delctx = (kumo_tcadb_del_ctx*)op;

	if( delctx->proc(delctx->casdata, (const char*)vbuf, vsiz) ) {
		delctx->deleted = true;
		return (void*)-1;  // delete it
	} else {
		return NULL;
	}
}

static bool kumo_tcadb_del(void* data,
		const char* key, uint32_t keylen,
		kumo_storage_casproc proc, void* casdata)
{
	kumo_tcadb* ctx = reinterpret_cast<kumo_tcadb*>(data);

	kumo_tcadb_del_ctx delctx = { true, proc, casdata };

	bool ret = tcadbputproc(ctx->db,
			key, keylen,
			NULL, 0,
			kumo_tcadb_del_proc,
			&delctx);

	if(!ret) {
		return false;
	}

	return delctx.deleted;
}


typedef struct {
	const char* val;
	uint32_t vallen;
	kumo_storage_casproc proc;
	void* casdata;
} kumo_tcadb_update_ctx;

static void* kumo_tcadb_update_proc(const void* vbuf, int vsiz, int *sp, void* op)
{
	kumo_tcadb_update_ctx* upctx = (kumo_tcadb_update_ctx*)op;

	if( upctx->proc(upctx->casdata, (const char*)vbuf, vsiz) ) {
		// update

		void* mem = ::malloc(upctx->vallen);
		if(!mem) {
			return NULL;  // FIXME
		}

		*sp = upctx->vallen;
		memcpy(mem, upctx->val, upctx->vallen);
		return mem;

	} else {
		// don't update
		return NULL;
	}
}

static bool kumo_tcadb_update(void* data,
			const char* key, uint32_t keylen,
			const char* val, uint32_t vallen,
			kumo_storage_casproc proc, void* casdata)
{
	kumo_tcadb* ctx = reinterpret_cast<kumo_tcadb*>(data);

	kumo_tcadb_update_ctx upctx = { val, vallen, proc, casdata };

	return tcadbputproc(ctx->db,
			key, keylen,
			val, vallen,
			kumo_tcadb_update_proc, &upctx);
}


static uint64_t kumo_tcadb_rnum(void* data)
{
	kumo_tcadb* ctx = reinterpret_cast<kumo_tcadb*>(data);
	return tcadbrnum(ctx->db);
}

static bool sync_rename(const char* src, const char* dst)
{
	int fd = ::open(src, O_WRONLY);
	if(fd < 0) {
		return false;
	}

	if(fsync(fd) < 0) {
		::close(fd);
		return false;
	}
	::close(fd);

	if(rename(src, dst) < 0) {
		return false;
	}

	return true;
}

static bool kumo_tcadb_backup(void* data, const char* dstpath)
{
	kumo_tcadb* ctx = reinterpret_cast<kumo_tcadb*>(data);

	char* tmppath = (char*)::malloc(
			strlen(dstpath)+strlen(BACKUP_TMP_SUFFIX));
	if(!tmppath) {
		return false;
	}

	::strcpy(tmppath, dstpath);
	::strcat(tmppath, BACKUP_TMP_SUFFIX);

	if(!tcadbcopy(ctx->db, tmppath)) {
		::free(tmppath);
		return false;
	}

	if(!sync_rename(tmppath, dstpath)) {
		::free(tmppath);
		return false;
	}
	::free(tmppath);

	return true;
}

static const char* kumo_tcadb_error(void* data)
{
	return "unknown error";
}


struct kumo_tcadb_iterator {
	const void* kbuf;
	int ksiz;
	const void* vbuf;
	int vsiz;
	kumo_tcadb* ctx;
};


typedef struct {
	int (*func)(void* user, void* iterator_data);
	void* user;
	kumo_tcadb* ctx;
	int err;
} kumo_tcadb_iter_ctx;

static bool kumo_tcadb_iter_proc(
		const void* kbuf, int ksiz,
		const void* vbuf, int vsiz, void* op)
{
	kumo_tcadb_iter_ctx* itctx = reinterpret_cast<kumo_tcadb_iter_ctx*>(op);

	kumo_tcadb_iterator it = { kbuf, ksiz, vbuf, vsiz, itctx->ctx };

	int ret = (*itctx->func)(itctx->user, (void*)&it);
	if(ret < 0) {
		itctx->err = ret;
		return false;
	}

	return true;
}


static int kumo_tcadb_for_each(void* data,
		void* user, int (*func)(void* user, void* iterator_data))
try {
	kumo_tcadb* ctx = reinterpret_cast<kumo_tcadb*>(data);

	// only one thread can use iterator
	mp::pthread_scoped_lock itlk(ctx->iterator_mutex);

	kumo_tcadb_iter_ctx itctx = { func, user, ctx, 0 };

	if(!tcadbforeach(ctx->db, kumo_tcadb_iter_proc, (void*)&itctx)) {
		return -1;
	}

	return itctx.err;

} catch (...) {
	return -1;
}

static const char* kumo_tcadb_iterator_key(void* iterator_data)
{
	kumo_tcadb_iterator* it = reinterpret_cast<kumo_tcadb_iterator*>(iterator_data);
	return (const char*)it->kbuf;
}

static const char* kumo_tcadb_iterator_val(void* iterator_data)
{
	kumo_tcadb_iterator* it = reinterpret_cast<kumo_tcadb_iterator*>(iterator_data);
	return (const char*)it->vbuf;
}

static size_t kumo_tcadb_iterator_keylen(void* iterator_data)
{
	kumo_tcadb_iterator* it = reinterpret_cast<kumo_tcadb_iterator*>(iterator_data);
	return it->ksiz;
}

static size_t kumo_tcadb_iterator_vallen(void* iterator_data)
{
	kumo_tcadb_iterator* it = reinterpret_cast<kumo_tcadb_iterator*>(iterator_data);
	return it->vsiz;
}


static const char* kumo_tcadb_iterator_release_key(void* iterator_data, msgpack_zone* zone)
{
	kumo_tcadb_iterator* it = reinterpret_cast<kumo_tcadb_iterator*>(iterator_data);

	void* ptr = ::malloc(it->ksiz);
	if(!ptr) {
		return NULL;
	}

	if(!msgpack_zone_push_finalizer(zone, &::free, ptr)) {
		::free(ptr);
		return NULL;
	}

	::memcpy(ptr, it->kbuf, it->ksiz);

	it->kbuf = NULL;
	return (const char*)ptr;
}

static const char* kumo_tcadb_iterator_release_val(void* iterator_data, msgpack_zone* zone)
{
	kumo_tcadb_iterator* it = reinterpret_cast<kumo_tcadb_iterator*>(iterator_data);

	void* ptr = ::malloc(it->vsiz);
	if(!ptr) {
		return NULL;
	}

	if(!msgpack_zone_push_finalizer(zone, &::free, ptr)) {
		::free(ptr);
		return NULL;
	}

	::memcpy(ptr, it->vbuf, it->vsiz);

	it->vbuf = NULL;
	return (const char*)ptr;
}

static bool kumo_tcadb_iterator_del(void* iterator_data,
		kumo_storage_casproc proc, void* casdata)
{
	// FIXME avoid deadlock
	//kumo_tcadb_iterator* it = reinterpret_cast<kumo_tcadb_iterator*>(iterator_data);
	return false;
}

static bool kumo_tcadb_iterator_del_force(void* iterator_data)
{
	// FIXME avoid deadlock
	//kumo_tcadb_iterator* it = reinterpret_cast<kumo_tcadb_iterator*>(iterator_data);
	return false;
}


static kumo_storage_op kumo_tcadb_op =
{
	kumo_tcadb_create,
	kumo_tcadb_free,
	kumo_tcadb_open,
	kumo_tcadb_close,
	kumo_tcadb_get,
	kumo_tcadb_set,
	kumo_tcadb_del,
	kumo_tcadb_update,
	NULL,
	kumo_tcadb_rnum,
	kumo_tcadb_backup,
	kumo_tcadb_error,
	kumo_tcadb_for_each,
	kumo_tcadb_iterator_key,
	kumo_tcadb_iterator_val,
	kumo_tcadb_iterator_keylen,
	kumo_tcadb_iterator_vallen,
	kumo_tcadb_iterator_release_key,
	kumo_tcadb_iterator_release_val,
	kumo_tcadb_iterator_del,
	kumo_tcadb_iterator_del_force,
};

kumo_storage_op kumo_storage_init(void)
{
	return kumo_tcadb_op;
}

