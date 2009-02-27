#include "storage.h"  // FIXME
#include <tchdb.h>

typedef struct {
	TCHDB* db;
	TCLIST* garbage;
} kumo_tchdb_ctx;


static void* kumo_tchdb_create(void)
{
	TCHDB* db = tchdbnew();
	return (void*)db;
}

static void kumo_tchdb_free(void* data)
{
	TCHDB* db = (TCHDB*)data;
	tchdbdel(db);
}

//static bool kumo_tchdb_open(void* data, int* argc, char** argv)
static bool kumo_tchdb_open(void* data, const char* path)
{
	TCHDB* db = (TCHDB*)data;

	//const char* path = argv[1];
	//*argc = 0;

	if(!tchdbsetmutex(db)) {
		return false;
	}

	if(!tchdbopen(db, path, HDBOWRITER|HDBOCREAT)) {
		return false;
	}

	/* FIXME
	struct timespec interval = { 24*60*60, 0 };
	kumo_start_timer(&interval, kumo_tchdb_timer, (void*)db);
	*/

	return true;
}

static void kumo_tchdb_close(void* data)
{
	TCHDB* db = (TCHDB*)data;
	tchdbclose(db);
}


static const char* kumo_tchdb_get(void* data,
		const char* key, uint32_t keylen,
		uint32_t* result_vallen,
		msgpack_zone* zone)
{
	TCHDB* db = (TCHDB*)data;

	int len;
	char* val = tchdbget(db, key, keylen, &len);
	if(!val) {
		return NULL;
	}

	msgpack_zone_push_finalizer(zone, free, val);
	return val;
}


//static void kumo_tchdb_timer(void* data)
//{
//	// FIXME garbage collect
//}


typedef struct {
	const void* val;
	uint32_t vallen;
} kumo_tchdb_update_ctx;

static void* kumo_tchdb_update_proc(const void* vbuf, int vsiz, int *sp, void* op)
{
	kumo_tchdb_update_ctx* ctx = (kumo_tchdb_update_ctx*)op;

	if(vsiz < 8 || kumo_clocktime_less(
			kumo_storage_clocktime_of(vbuf),
			kumo_storage_clocktime_of(ctx->val))) {

		void* mem = malloc(ctx->vallen);
		if(!mem) {
			return NULL;  // FIXME
		}

		*sp = ctx->vallen;
		memcpy(mem, ctx->val, ctx->vallen);
		return mem;

	} else {
		return NULL;
	}
}


static bool kumo_tchdb_set(void* data,
		const char* key, uint32_t keylen,
		const char* val, uint32_t vallen)
{
	TCHDB* db = (TCHDB*)data;
	return tchdbput(db, key, keylen, val, vallen);
}

static bool kumo_tchdb_update(void* data,
		const char* key, uint32_t keylen,
		const char* val, uint32_t vallen)
{
	TCHDB* db = (TCHDB*)data;

	kumo_tchdb_update_ctx ctx = { val, vallen };

	return tchdbputproc(db,
			key, keylen,
			val, vallen,
			kumo_tchdb_update_proc, &ctx);
}

static bool kumo_tchdb_del(void* data,
		const char* key, uint32_t keylen,
		uint64_t clocktime)
{
	TCHDB* db = (TCHDB*)data;

	char clockbuf[8];
	kumo_storage_clocktime_to(clocktime, clockbuf);

	kumo_tchdb_update_ctx ctx = { clockbuf, 8 };

	return tchdbputproc(db,
			key, keylen,
			clockbuf, 8,
			kumo_tchdb_update_proc, &ctx);
}

static uint64_t kumo_tchdb_rnum(void* data)
{
	TCHDB* db = (TCHDB*)data;
	return tchdbrnum(db);
}

static bool kumo_tchdb_backup(void* data, const char* dstpath)
{
	TCHDB* db = (TCHDB*)data;
	return tchdbcopy(db, dstpath);
}

static const char* kumo_tchdb_error(void* data)
{
	TCHDB* db = (TCHDB*)data;
	return tchdberrmsg(tchdbecode(db));
}


typedef struct {
	TCXSTR* key;
	TCXSTR* val;
	TCHDB* db;
} kumo_tchdb_iterator;

static int kumo_tchdb_for_each(void* data,
		void* user, int (*func)(void* user, void* iterator_data))
{
	// FIXME only one thread can use iterator. use mutex.

	TCHDB* db = (TCHDB*)data;

	tchdbiterinit(db);

	kumo_tchdb_iterator it = { NULL, NULL, NULL };
	it.key = tcxstrnew(); if(!it.key) { return -1; }
	it.val = tcxstrnew(); if(!it.val) { tcxstrdel(it.key); return -1; }
	it.db  = db;

	while( tchdbiternext3(db, it.key, it.val) ) {
		if(TCXSTRSIZE(it.val) < 16 || TCXSTRSIZE(it.key) < 8) {
			// FIXME delete it?
			continue;
		}

		int ret = (*func)(user, (void*)&it);
		if(ret < 0) {
			if(it.key != NULL) { tcxstrdel(it.key); }
			if(it.val != NULL) { tcxstrdel(it.val); }
			return ret;
		}

		if(it.key == NULL) {
			it.key = tcxstrnew();
			if(it.key == NULL) {
				if(it.val != NULL) { tcxstrdel(it.val); }
				return -1;
			}
		}

		if(it.val == NULL) {
			it.val = tcxstrnew();
			if(it.val == NULL) {
				tcxstrdel(it.key);
				return -1;
			}
		}
	}

	if(it.key != NULL) { tcxstrdel(it.key); }
	if(it.val != NULL) { tcxstrdel(it.val); }
	return 0;
}

static const char* kumo_tchdb_iterator_key(void* iterator_data)
{
	kumo_tchdb_iterator* it = (kumo_tchdb_iterator*)iterator_data;
	return TCXSTRPTR(it->key);
}

static const char* kumo_tchdb_iterator_val(void* iterator_data)
{
	kumo_tchdb_iterator* it = (kumo_tchdb_iterator*)iterator_data;
	return TCXSTRPTR(it->val);
}

static size_t kumo_tchdb_iterator_keylen(void* iterator_data)
{
	kumo_tchdb_iterator* it = (kumo_tchdb_iterator*)iterator_data;
	return TCXSTRSIZE(it->key);
}

static size_t kumo_tchdb_iterator_vallen(void* iterator_data)
{
	kumo_tchdb_iterator* it = (kumo_tchdb_iterator*)iterator_data;
	return TCXSTRSIZE(it->val);
}


static bool kumo_tchdb_iterator_release_key(void* iterator_data, msgpack_zone* zone)
{
	kumo_tchdb_iterator* it = (kumo_tchdb_iterator*)iterator_data;

	if(!msgpack_zone_push_finalizer(zone, (void (*)(void*))tcxstrdel, it->key)) {
		return false;
	}

	it->key = NULL;
	return true;
}

static bool kumo_tchdb_iterator_release_val(void* iterator_data, msgpack_zone* zone)
{
	kumo_tchdb_iterator* it = (kumo_tchdb_iterator*)iterator_data;

	if(!msgpack_zone_push_finalizer(zone, (void (*)(void*))tcxstrdel, it->val)) {
		return false;
	}

	it->val = NULL;
	return true;
}

static bool kumo_tchdb_iterator_delete(void* iterator_data)
{
	kumo_tchdb_iterator* it = (kumo_tchdb_iterator*)iterator_data;

	const char* key = TCXSTRPTR(it->key);
	size_t keylen = TCXSTRSIZE(it->key);

	return tchdbout(it->db, key, keylen);
}

static bool kumo_tchdb_iterator_delete_if_older(void* iterator_data, uint64_t if_older)
{
	kumo_tchdb_iterator* it = (kumo_tchdb_iterator*)iterator_data;

	const char* val = TCXSTRPTR(it->val);
	size_t vallen = TCXSTRSIZE(it->val);

	if(vallen < 8 || kumo_clocktime_less(
				kumo_storage_clocktime_of(val),
				if_older)) {

		const char* key = TCXSTRPTR(it->key);
		size_t keylen = TCXSTRSIZE(it->key);

		return tchdbout(it->db, key, keylen);
	}

	return false;
}


static kumo_storage_op kumo_tchdb_op =
{
	kumo_tchdb_create,
	kumo_tchdb_free,
	kumo_tchdb_open,
	kumo_tchdb_close,
	kumo_tchdb_get,
	kumo_tchdb_set,
	kumo_tchdb_update,
	NULL,
	kumo_tchdb_del,
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
	kumo_tchdb_iterator_delete,
	kumo_tchdb_iterator_delete_if_older,
};

kumo_storage_op kumo_storage_init(void)
{
	return kumo_tchdb_op;
}

