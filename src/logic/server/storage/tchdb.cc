#include "interface.h"  // FIXME
#include <tchdb.h>
#include <mp/pthread.h>


struct kumo_tchdb {
	kumo_tchdb()
	{
		db = tchdbnew();
		if(!db) {
			throw std::bad_alloc();
		}
		//tchdbsetcache(db, 32000);   FIXME
		//tchdbsetxmsiz(db, 1024*1024);   FIXME
	}

	~kumo_tchdb()
	{
		tchdbdel(db);
	}

	TCHDB* db;
	mp::pthread_mutex iterator_mutex;

private:
	kumo_tchdb(const kumo_tchdb&);
};


static void* kumo_tchdb_create(void)
try {
	kumo_tchdb* ctx = new kumo_tchdb();
	return reinterpret_cast<void*>(ctx);

} catch (...) {
	return NULL;
}

static void kumo_tchdb_free(void* data)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);
	delete ctx;
}

static bool kumo_tchdb_open(void* data, const char* path)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);

	if(!tchdbsetmutex(ctx->db)) {
		return false;
	}

	if(!tchdbopen(ctx->db, path, HDBOWRITER|HDBOCREAT)) {
		return false;
	}

	return true;
}

static void kumo_tchdb_close(void* data)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);
	tchdbclose(ctx->db);
}


static const char* kumo_tchdb_get(void* data,
		const char* key, uint32_t keylen,
		uint32_t* result_vallen,
		msgpack_zone* zone)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);

	int len;
	char* val = (char*)tchdbget(ctx->db, key, keylen, &len);
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

static bool kumo_tchdb_set(void* data,
		const char* key, uint32_t keylen,
		const char* val, uint32_t vallen)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);
	return tchdbput(ctx->db, key, keylen, val, vallen);
}


static bool kumo_tchdb_del(void* data,
		const char* key, uint32_t keylen,
		kumo_storage_casproc proc, void* casdata)
{
	// FIXME tchdboutproc
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);
	return tchdbout(ctx->db, key, keylen);
}


typedef struct {
	const char* val;
	uint32_t vallen;
	kumo_storage_casproc proc;
	void* casdata;
} kumo_tchdb_update_ctx;

static void* kumo_tchdb_update_proc(const void* vbuf, int vsiz, int *sp, void* op)
{
	kumo_tchdb_update_ctx* upctx = (kumo_tchdb_update_ctx*)op;

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

static bool kumo_tchdb_update(void* data,
			const char* key, uint32_t keylen,
			const char* val, uint32_t vallen,
			kumo_storage_casproc proc, void* casdata)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);

	kumo_tchdb_update_ctx upctx = { val, vallen, proc, casdata };

	return tchdbputproc(ctx->db,
			key, keylen,
			val, vallen,
			kumo_tchdb_update_proc, &upctx);
}


static uint64_t kumo_tchdb_rnum(void* data)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);
	return tchdbrnum(ctx->db);
}

static bool kumo_tchdb_backup(void* data, const char* dstpath)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);
	return tchdbcopy(ctx->db, dstpath);
}

static const char* kumo_tchdb_error(void* data)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);
	return tchdberrmsg(tchdbecode(ctx->db));
}


struct kumo_tchdb_iterator {
	kumo_tchdb_iterator(kumo_tchdb* pctx) :
		ctx(pctx)
	{
		key = tcxstrnew();
		if(!key) {
			throw std::bad_alloc();
		}
		val = tcxstrnew();
		if(!val) {
			tcxstrdel(key);
			throw std::bad_alloc();
		}
	}

	~kumo_tchdb_iterator()
	{
		if(key != NULL) { tcxstrdel(key); }
		if(val != NULL) { tcxstrdel(val); }
	}

	void reset()
	{
		if(!key) {
			key = tcxstrnew();
			if(!key) {
				throw std::bad_alloc();
			}
		}

		if(!val) {
			val = tcxstrnew();
			if(!val) {
				throw std::bad_alloc();
			}
		}
	}

	TCXSTR* key;
	TCXSTR* val;
	kumo_tchdb* ctx;

private:
	kumo_tchdb_iterator();
	kumo_tchdb_iterator(const kumo_tchdb_iterator&);
};

static int kumo_tchdb_for_each(void* data,
		void* user, int (*func)(void* user, void* iterator_data))
try {
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);

	// only one thread can use iterator
	mp::pthread_scoped_lock itlk(ctx->iterator_mutex);

	if(!tchdbiterinit(ctx->db)) {
		return -1;
	}

	kumo_tchdb_iterator it(ctx);

	while( tchdbiternext3(ctx->db, it.key, it.val) ) {
		int ret = (*func)(user, (void*)&it);
		if(ret < 0) {
			return ret;
		}

		it.reset();
	}

	return 0;

} catch (...) {
	return -1;
}

static const char* kumo_tchdb_iterator_key(void* iterator_data)
{
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);
	return TCXSTRPTR(it->key);
}

static const char* kumo_tchdb_iterator_val(void* iterator_data)
{
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);
	return TCXSTRPTR(it->val);
}

static size_t kumo_tchdb_iterator_keylen(void* iterator_data)
{
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);
	return TCXSTRSIZE(it->key);
}

static size_t kumo_tchdb_iterator_vallen(void* iterator_data)
{
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);
	return TCXSTRSIZE(it->val);
}


static bool kumo_tchdb_iterator_release_key(void* iterator_data, msgpack_zone* zone)
{
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);

	if(!msgpack_zone_push_finalizer(zone, (void (*)(void*))tcxstrdel, it->key)) {
		return false;
	}

	it->key = NULL;
	return true;
}

static bool kumo_tchdb_iterator_release_val(void* iterator_data, msgpack_zone* zone)
{
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);

	if(!msgpack_zone_push_finalizer(zone, (void (*)(void*))tcxstrdel, it->val)) {
		return false;
	}

	it->val = NULL;
	return true;
}

static bool kumo_tchdb_iterator_del(void* iterator_data,
		kumo_storage_casproc proc, void* casdata)
{
	// FIXME tchdboutproc
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);

	const char* key = TCXSTRPTR(it->key);
	size_t keylen = TCXSTRSIZE(it->key);

	return tchdbout(it->ctx->db, key, keylen);
}

static bool kumo_tchdb_iterator_del_force(void* iterator_data)
{
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);

	const char* key = TCXSTRPTR(it->key);
	size_t keylen = TCXSTRSIZE(it->key);

	return tchdbout(it->ctx->db, key, keylen);
}


static kumo_storage_op kumo_tchdb_op =
{
	kumo_tchdb_create,
	kumo_tchdb_free,
	kumo_tchdb_open,
	kumo_tchdb_close,
	kumo_tchdb_get,
	kumo_tchdb_set,
	kumo_tchdb_del,
	kumo_tchdb_update,
	NULL,
	kumo_tchdb_rnum,
	kumo_tchdb_backup,
	kumo_tchdb_error,
	kumo_tchdb_for_each,
	kumo_tchdb_iterator_key,
	kumo_tchdb_iterator_val,
	kumo_tchdb_iterator_keylen,
	kumo_tchdb_iterator_vallen,
	kumo_tchdb_iterator_release_key,
	kumo_tchdb_iterator_release_val,
	kumo_tchdb_iterator_del,
	kumo_tchdb_iterator_del_force,
};

kumo_storage_op kumo_storage_init(void)
{
	return kumo_tchdb_op;
}

