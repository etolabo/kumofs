//
// kumofs
//
// Copyright (C) 2009 Etolabo Corp.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#include "storage/interface.h"  // FIXME
#include <tchdb.h>
#include <tcutil.h>
#include <mp/pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BACKUP_TMP_SUFFIX ".tmp"

static char* parse_param(char* str,
		bool* rcnum_set, int32_t* rcnum,
		bool* xmsiz_set, int64_t* xmsiz)
{
	char* key;
	char* val;
	while((key = ::strrchr(str, '#')) != NULL) {
		*key++ = '\0';
		if((val = strchr(key, '=')) == NULL) {
			return NULL;
		}
		*val++ = '\0';

		if(::strcmp(key, "rcnum") == 0) {
			*rcnum_set = true;
			*rcnum = tcatoix(val);  // FIXME error check?
		} else if(::strcmp(key, "xmsiz") == 0) {
			*xmsiz_set = true;
			*xmsiz = tcatoix(val);  // FIXME error check?
		}
	}
	return str;
}


struct kumo_tchdb {
	kumo_tchdb()
	{
		db = tchdbnew();
		if(!db) {
			throw std::bad_alloc();
		}

		if(!tchdbsetmutex(db)) {
			tchdbdel(db);
			throw std::bad_alloc();
		}
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

	char* str = ::strdup(path);
	if(!str) {
		return false;
	}

	int32_t rcnum;  bool rcnum_set = false;
	int64_t xmsiz;  bool xmsiz_set = false;

	path = parse_param(str,
			&rcnum_set, &rcnum,
			&xmsiz_set, &xmsiz);
	if(!path) {
		goto param_error;
	}

	if(rcnum_set && !tchdbsetcache(ctx->db, rcnum)) {
		goto param_error;
	}

	if(xmsiz_set && !tchdbsetxmsiz(ctx->db, xmsiz)) {
		goto param_error;
	}

	if(!tchdbopen(ctx->db, path, HDBOWRITER|HDBOCREAT)) {
		goto param_error;
	}

	::free(str);
	return true;

param_error:
	::free(str);
	return false;
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

static int32_t kumo_tchdb_get_header(void* data,
		const char* key, uint32_t keylen,
		char* result_val, uint32_t vallen)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);
	return tchdbget3(ctx->db, key, keylen, result_val, vallen);
}

static bool kumo_tchdb_set(void* data,
		const char* key, uint32_t keylen,
		const char* val, uint32_t vallen)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);
	return tchdbput(ctx->db, key, keylen, val, vallen);
}


typedef struct {
	bool deleted;
	kumo_storage_casproc proc;
	void* casdata;
} kumo_tchdb_del_ctx;

static void* kumo_tchdb_del_proc(const void* vbuf, int vsiz, int* sp, void* op)
{
	kumo_tchdb_del_ctx* delctx = (kumo_tchdb_del_ctx*)op;

	if( delctx->proc(delctx->casdata, (const char*)vbuf, vsiz) ) {
		delctx->deleted = true;
		return (void*)-1;  // delete it
	} else {
		return NULL;
	}
}

static bool kumo_tchdb_del(void* data,
		const char* key, uint32_t keylen,
		kumo_storage_casproc proc, void* casdata)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);

	kumo_tchdb_del_ctx delctx = { true, proc, casdata };

	bool ret = tchdbputproc(ctx->db,
			key, keylen,
			NULL, 0,
			kumo_tchdb_del_proc,
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

static bool kumo_tchdb_backup(void* data, const char* dstpath)
{
	kumo_tchdb* ctx = reinterpret_cast<kumo_tchdb*>(data);

	char* tmppath = (char*)::malloc(
			strlen(dstpath)+strlen(BACKUP_TMP_SUFFIX));
	if(!tmppath) {
		return false;
	}

	::strcpy(tmppath, dstpath);
	::strcat(tmppath, BACKUP_TMP_SUFFIX);

	if(!tchdbcopy(ctx->db, tmppath)) {
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


static const char* kumo_tchdb_iterator_release_key(void* iterator_data, msgpack_zone* zone)
{
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);

	if(!msgpack_zone_push_finalizer(zone, (void (*)(void*))tcxstrdel, it->key)) {
		return NULL;
	}

	const char* tmp = TCXSTRPTR(it->key);
	it->key = NULL;
	return tmp;
}

static const char* kumo_tchdb_iterator_release_val(void* iterator_data, msgpack_zone* zone)
{
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);

	if(!msgpack_zone_push_finalizer(zone, (void (*)(void*))tcxstrdel, it->val)) {
		return NULL;
	}

	const char* tmp = TCXSTRPTR(it->val);
	it->val = NULL;
	return tmp;
}

static bool kumo_tchdb_iterator_del(void* iterator_data,
		kumo_storage_casproc proc, void* casdata)
{
	kumo_tchdb_iterator* it = reinterpret_cast<kumo_tchdb_iterator*>(iterator_data);

	const char* key = TCXSTRPTR(it->key);
	size_t keylen = TCXSTRSIZE(it->key);

	return kumo_tchdb_del(it->ctx, key, keylen, proc, casdata);
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
	kumo_tchdb_get_header,
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

