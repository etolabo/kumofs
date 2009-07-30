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
#include <tcbdb.h>
#include <tcutil.h>
#include <mp/pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BACKUP_TMP_SUFFIX ".tmp"

static char* parse_param(char* str,
		bool* lcnum_set, int32_t* lcnum,
		bool* ncnum_set, int32_t* ncnum,
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

		if(::strcmp(key, "lcnum") == 0) {
			*lcnum_set = true;
			*lcnum = tcatoix(val);  // FIXME error check?
		} else if(::strcmp(key, "ncnum") == 0) {
			*ncnum_set = true;
			*ncnum = tcatoix(val);  // FIXME error check?
		} else if(::strcmp(key, "xmsiz") == 0) {
			*xmsiz_set = true;
			*xmsiz = tcatoix(val);  // FIXME error check?
		}
	}
	return str;
}


struct kumo_tcbdb {
	kumo_tcbdb()
	{
		db = tcbdbnew();
		if(!db) {
			throw std::bad_alloc();
		}

		if(!tcbdbsetmutex(db)) {
			tcbdbdel(db);
			throw std::bad_alloc();
		}
	}

	~kumo_tcbdb()
	{
		tcbdbdel(db);
	}

	TCBDB* db;

private:
	kumo_tcbdb(const kumo_tcbdb&);
};


static void* kumo_tcbdb_create(void)
try {
	kumo_tcbdb* ctx = new kumo_tcbdb();
	return reinterpret_cast<void*>(ctx);

} catch (...) {
	return NULL;
}

static void kumo_tcbdb_free(void* data)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);
	delete ctx;
}

static bool kumo_tcbdb_open(void* data, const char* path)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);

	int32_t lcnum = -1;
	int32_t ncnum = -1;  bool cnum_set = false;
	int64_t xmsiz;       bool xmsiz_set = false;

	char* str = ::strdup(path);
	if(!str) {
		return false;
	}

	path = parse_param(str,
			&cnum_set, &lcnum,
			&cnum_set, &ncnum,
			&xmsiz_set, &xmsiz);
	if(!path) {
		goto param_error;
	}

	if(cnum_set && !tcbdbsetcache(ctx->db, lcnum, ncnum)) {
		goto param_error;
	}

	if(xmsiz_set && !tcbdbsetxmsiz(ctx->db, xmsiz)) {
		goto param_error;
	}

	if(!tcbdbopen(ctx->db, path, BDBOWRITER|BDBOCREAT)) {
		goto param_error;
	}

	::free(str);
	return true;

param_error:
	::free(str);
	return false;
}

static void kumo_tcbdb_close(void* data)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);
	tcbdbclose(ctx->db);
}


static const char* kumo_tcbdb_get(void* data,
		const char* key, uint32_t keylen,
		uint32_t* result_vallen,
		msgpack_zone* zone)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);

	int len;
	char* val = (char*)tcbdbget(ctx->db, key, keylen, &len);
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

static int32_t kumo_tcbdb_get_header(void* data,
		const char* key, uint32_t keylen,
		char* result_val, uint32_t vallen)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);

	// FIXME thread safety
	int len;
	char* val = (char*)tcbdbget(ctx->db, key, keylen, &len);
	if(!val) {
		return NULL;
	}

	if((uint32_t)len < vallen) {
		memcpy(result_val, val, len);
		return len;
	}
	memcpy(result_val, val, vallen);
	return vallen;
}

static bool kumo_tcbdb_set(void* data,
		const char* key, uint32_t keylen,
		const char* val, uint32_t vallen)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);
	return tcbdbput(ctx->db, key, keylen, val, vallen);
}


typedef struct {
	bool deleted;
	kumo_storage_casproc proc;
	void* casdata;
} kumo_tcbdb_del_ctx;

static void* kumo_tcbdb_del_proc(const void* vbuf, int vsiz, int* sp, void* op)
{
	kumo_tcbdb_del_ctx* delctx = (kumo_tcbdb_del_ctx*)op;

	if( delctx->proc(delctx->casdata, (const char*)vbuf, vsiz) ) {
		delctx->deleted = true;
		return (void*)-1;  // delete it
	} else {
		return NULL;
	}
}

static bool kumo_tcbdb_del(void* data,
		const char* key, uint32_t keylen,
		kumo_storage_casproc proc, void* casdata)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);

	kumo_tcbdb_del_ctx delctx = { true, proc, casdata };

	bool ret = tcbdbputproc(ctx->db,
			key, keylen,
			NULL, 0,
			kumo_tcbdb_del_proc,
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
} kumo_tcbdb_update_ctx;

static void* kumo_tcbdb_update_proc(const void* vbuf, int vsiz, int *sp, void* op)
{
	kumo_tcbdb_update_ctx* upctx = (kumo_tcbdb_update_ctx*)op;

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

static bool kumo_tcbdb_update(void* data,
			const char* key, uint32_t keylen,
			const char* val, uint32_t vallen,
			kumo_storage_casproc proc, void* casdata)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);

	kumo_tcbdb_update_ctx upctx = { val, vallen, proc, casdata };

	return tcbdbputproc(ctx->db,
			key, keylen,
			val, vallen,
			kumo_tcbdb_update_proc, &upctx);
}


static uint64_t kumo_tcbdb_rnum(void* data)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);
	return tcbdbrnum(ctx->db);
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

static bool kumo_tcbdb_backup(void* data, const char* dstpath)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);

	char* tmppath = (char*)::malloc(
			strlen(dstpath)+strlen(BACKUP_TMP_SUFFIX));
	if(!tmppath) {
		return false;
	}

	::strcpy(tmppath, dstpath);
	::strcat(tmppath, BACKUP_TMP_SUFFIX);

	if(!tcbdbcopy(ctx->db, tmppath)) {
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

static const char* kumo_tcbdb_error(void* data)
{
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);
	return tcbdberrmsg(tcbdbecode(ctx->db));
}


struct kumo_tcbdb_iterator {
	kumo_tcbdb_iterator(BDBCUR* c) :
		cur(c)
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

	~kumo_tcbdb_iterator()
	{
		if(key != NULL) { tcxstrdel(key); }
		if(val != NULL) { tcxstrdel(val); }
		tcbdbcurdel(cur);
	}

	bool next()
	{
		return tcbdbcurnext(cur);
	}

	void fetch()
	{
		if(!tcbdbcurrec(cur, key, val)) {
			throw std::bad_alloc();
		}
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

		deleted = false;
	}

	bool del()
	{
		if(tcbdbcurout(cur)) {
			// FIXME if the cursor is on last position?
			deleted = true;
			return true;
		} else {
			return false;
		}
	}

	TCXSTR* key;
	TCXSTR* val;
	BDBCUR* cur;
	bool deleted;

private:
	kumo_tcbdb_iterator();
	kumo_tcbdb_iterator(const kumo_tcbdb_iterator&);
};

static int kumo_tcbdb_for_each(void* data,
		void* user, int (*func)(void* user, void* iterator_data))
try {
	kumo_tcbdb* ctx = reinterpret_cast<kumo_tcbdb*>(data);

	BDBCUR* cur = tcbdbcurnew(ctx->db);

	if(!tcbdbcurfirst(cur)) {
		return 0;
	}

	kumo_tcbdb_iterator it(cur);

	while(true) {
		it.fetch();

		int ret = (*func)(user, (void*)&it);
		if(ret < 0) {
			return ret;
		}

		if(!it.deleted) {
			if(!it.next()) {
				return 0;
			}
		}

		it.reset();
	}

	return 0;

} catch (...) {
	return -1;
}

static const char* kumo_tcbdb_iterator_key(void* iterator_data)
{
	kumo_tcbdb_iterator* it = reinterpret_cast<kumo_tcbdb_iterator*>(iterator_data);
	return TCXSTRPTR(it->key);
}

static const char* kumo_tcbdb_iterator_val(void* iterator_data)
{
	kumo_tcbdb_iterator* it = reinterpret_cast<kumo_tcbdb_iterator*>(iterator_data);
	return TCXSTRPTR(it->val);
}

static size_t kumo_tcbdb_iterator_keylen(void* iterator_data)
{
	kumo_tcbdb_iterator* it = reinterpret_cast<kumo_tcbdb_iterator*>(iterator_data);
	return TCXSTRSIZE(it->key);
}

static size_t kumo_tcbdb_iterator_vallen(void* iterator_data)
{
	kumo_tcbdb_iterator* it = reinterpret_cast<kumo_tcbdb_iterator*>(iterator_data);
	return TCXSTRSIZE(it->val);
}


static const char* kumo_tcbdb_iterator_release_key(void* iterator_data, msgpack_zone* zone)
{
	kumo_tcbdb_iterator* it = reinterpret_cast<kumo_tcbdb_iterator*>(iterator_data);

	if(!msgpack_zone_push_finalizer(zone, (void (*)(void*))tcxstrdel, it->key)) {
		return NULL;
	}

	const char* tmp = TCXSTRPTR(it->key);
	it->key = NULL;
	return tmp;
}

static const char* kumo_tcbdb_iterator_release_val(void* iterator_data, msgpack_zone* zone)
{
	kumo_tcbdb_iterator* it = reinterpret_cast<kumo_tcbdb_iterator*>(iterator_data);

	if(!msgpack_zone_push_finalizer(zone, (void (*)(void*))tcxstrdel, it->val)) {
		return NULL;
	}

	const char* tmp = TCXSTRPTR(it->val);
	it->val = NULL;
	return tmp;
}

static bool kumo_tcbdb_iterator_del(void* iterator_data,
		kumo_storage_casproc proc, void* casdata)
{
	// FIXME not supported
	//kumo_tcbdb_iterator* it = reinterpret_cast<kumo_tcbdb_iterator*>(iterator_data);
	return false;
}

static bool kumo_tcbdb_iterator_del_force(void* iterator_data)
{
	kumo_tcbdb_iterator* it = reinterpret_cast<kumo_tcbdb_iterator*>(iterator_data);
	return it->del();
}


static kumo_storage_op kumo_tcbdb_op =
{
	kumo_tcbdb_create,
	kumo_tcbdb_free,
	kumo_tcbdb_open,
	kumo_tcbdb_close,
	kumo_tcbdb_get,
	kumo_tcbdb_get_header,
	kumo_tcbdb_set,
	kumo_tcbdb_del,
	kumo_tcbdb_update,
	NULL,
	kumo_tcbdb_rnum,
	kumo_tcbdb_backup,
	kumo_tcbdb_error,
	kumo_tcbdb_for_each,
	kumo_tcbdb_iterator_key,
	kumo_tcbdb_iterator_val,
	kumo_tcbdb_iterator_keylen,
	kumo_tcbdb_iterator_vallen,
	kumo_tcbdb_iterator_release_key,
	kumo_tcbdb_iterator_release_val,
	kumo_tcbdb_iterator_del,
	kumo_tcbdb_iterator_del_force,
};

kumo_storage_op kumo_storage_init(void)
{
	return kumo_tcbdb_op;
}

