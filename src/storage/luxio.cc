#include "storage/interface.h"  // FIXME
#include <memory>
#include <pthread.h>
#include <luxio/btree.h>

static __thread std::string *s_error = NULL;

static void set_error(const std::string& msg)
try {
	if(!s_error) {
		s_error = new std::string;
	}
	*s_error = msg;

} catch (...) { }

static void* kumo_luxio_create(void)
try {
	Lux::IO::Btree* db = new Lux::IO::Btree(Lux::IO::CLUSTER);

	// FIXME LOCK_NONE + rwlock
	db->set_lock_type(Lux::IO::LOCK_THREAD);
	return reinterpret_cast<void*>(db);

} catch (std::exception& e) {
	set_error(e.what());
	return NULL;
}

static void kumo_luxio_free(void* data)
try {
	Lux::IO::Btree* db = reinterpret_cast<Lux::IO::Btree*>(data);

	delete db;

} catch (std::exception& e) {
	set_error(e.what());
}


static bool kumo_luxio_open(void* data, const char* path)
try {
	Lux::IO::Btree* db = reinterpret_cast<Lux::IO::Btree*>(data);

	return db->open(path, Lux::DB_CREAT);

} catch (std::exception& e) {
	set_error(e.what());
	return false;
}


static void kumo_luxio_close(void* data)
try {
	Lux::IO::Btree* db = reinterpret_cast<Lux::IO::Btree*>(data);

	db->close();

} catch (std::exception& e) {
	set_error(e.what());
}



static void kumo_luxio_clean_data(void* val)
{
	// FIXME
	reinterpret_cast<Lux::IO::Btree*>(NULL)->clean_data(
			reinterpret_cast<Lux::IO::data_t*>(val));
}

static const char* kumo_luxio_get(void* data,
		const char* key, uint32_t keylen,
		uint32_t* result_vallen,
		msgpack_zone* zone)
try {
	Lux::IO::Btree* db = reinterpret_cast<Lux::IO::Btree*>(data);

	Lux::IO::data_t k = {key, keylen};
	Lux::IO::data_t* v = NULL;
	if(!db->get(&k, &v, Lux::IO::SYSTEM) || v == NULL) {
		return NULL;
	}

	if(!msgpack_zone_push_finalizer(
				zone, kumo_luxio_clean_data, v)) {
		db->clean_data(v);
		return NULL;
	}

	*result_vallen = v->size;
	return (const char*)v->data;

} catch (std::exception& e) {
	set_error(e.what());
	return NULL;
}


static bool kumo_luxio_set(void* data,
		const char* key, uint32_t keylen,
		const char* val, uint32_t vallen)
try {
	Lux::IO::Btree* db = reinterpret_cast<Lux::IO::Btree*>(data);

	return db->put(key, keylen, val, vallen);

} catch (std::exception& e) {
	set_error(e.what());
	return false;
}


static bool kumo_luxio_update(void* data,
		const char* key, uint32_t keylen,
		const char* val, uint32_t vallen)
try {
	// FIXME
	return kumo_luxio_set(data, key, keylen, val, vallen);

} catch (std::exception& e) {
	set_error(e.what());
	return false;
}

static bool kumo_luxio_del(void* data,
		const char* key, uint32_t keylen,
		uint64_t clocktime)
try {
	Lux::IO::Btree* db = reinterpret_cast<Lux::IO::Btree*>(data);

	Lux::IO::data_t k = {key, keylen};
	// FIXME
	return db->del(&k);

} catch (std::exception& e) {
	set_error(e.what());
	return false;
}


static uint64_t kumo_luxio_rnum(void* data)
try {
	Lux::IO::Btree* db = reinterpret_cast<Lux::IO::Btree*>(data);

	// FIXME
	return 0;

} catch (std::exception& e) {
	set_error(e.what());
	return 0;
}


static bool kumo_luxio_backup(void* data, const char* dstpath)
try {
	Lux::IO::Btree* db = reinterpret_cast<Lux::IO::Btree*>(data);

	// FIXME
	return false;

} catch (std::exception& e) {
	set_error(e.what());
	return false;
}


static const char* kumo_luxio_error(void* data)
{
	if(s_error) {
		return s_error->c_str();
	} else {
		return "";
	}
}


struct kumo_luxio_iterator {
	kumo_luxio_iterator(Lux::IO::Btree* pdb) :
		key(NULL), val(NULL), db(pdb) { }

	~kumo_luxio_iterator()
	{
		if(key) { db->clean_data(key); }
		if(val) { db->clean_data(val); }
	}

	Lux::IO::data_t* key;
	Lux::IO::data_t* val;
	Lux::IO::Btree* db;

private:
	kumo_luxio_iterator();
	kumo_luxio_iterator(const kumo_luxio_iterator&);
};

static int kumo_luxio_for_each(void* data,
		void* user, int (*func)(void* user, void* iterator_data))
try {
	Lux::IO::Btree* db = reinterpret_cast<Lux::IO::Btree*>(data);

	kumo_luxio_iterator it(db);

	std::auto_ptr<Lux::IO::cursor_t> cur( db->cursor_init() );

	if(!db->first(cur.get())) {
		return 0;
	}

	do {
		if(!db->cursor_get(cur.get(), &it.key, &it.val, Lux::IO::SYSTEM)) {
			continue;
		}

		if(it.val->size < 16 || it.key->size < 8) {
			// FIXME delete it?
			continue;
		}

		int ret = (*func)(user, reinterpret_cast<void*>(&it));
		if(ret < 0) {
			return ret;
		}

	} while(db->next(cur.get()));

	return 0;

} catch (std::exception& e) {
	set_error(e.what());
	return -1;
}


static const char* kumo_luxio_iterator_key(void* iterator_data)
try {
	kumo_luxio_iterator* it =
		reinterpret_cast<kumo_luxio_iterator*>(iterator_data);

	return (const char*)it->key->data;

} catch (std::exception& e) {
	set_error(e.what());
	return NULL;
}


static const char* kumo_luxio_iterator_val(void* iterator_data)
try {
	kumo_luxio_iterator* it =
		reinterpret_cast<kumo_luxio_iterator*>(iterator_data);

	return (const char*)it->val->data;

} catch (std::exception& e) {
	set_error(e.what());
	return NULL;
}


static size_t kumo_luxio_iterator_keylen(void* iterator_data)
try {
	kumo_luxio_iterator* it =
		reinterpret_cast<kumo_luxio_iterator*>(iterator_data);

	return it->key->size;

} catch (std::exception& e) {
	set_error(e.what());
	return NULL;
}


static size_t kumo_luxio_iterator_vallen(void* iterator_data)
try {
	kumo_luxio_iterator* it =
		reinterpret_cast<kumo_luxio_iterator*>(iterator_data);

	return it->val->size;

} catch (std::exception& e) {
	set_error(e.what());
	return NULL;
}


static bool kumo_luxio_iterator_release_key(void* iterator_data, msgpack_zone* zone)
try {
	kumo_luxio_iterator* it =
		reinterpret_cast<kumo_luxio_iterator*>(iterator_data);

	if(!msgpack_zone_push_finalizer(
				zone, kumo_luxio_clean_data, it->key)) {
		return false;
	}

	it->key = NULL;
	return true;

} catch (std::exception& e) {
	set_error(e.what());
	return NULL;
}


static bool kumo_luxio_iterator_release_val(void* iterator_data, msgpack_zone* zone)
try {
	kumo_luxio_iterator* it =
		reinterpret_cast<kumo_luxio_iterator*>(iterator_data);

	if(!msgpack_zone_push_finalizer(
				zone, kumo_luxio_clean_data, it->val)) {
		return false;
	}

	it->val = NULL;
	return true;

} catch (std::exception& e) {
	set_error(e.what());
	return NULL;
}


static bool kumo_luxio_iterator_del(void* iterator_data)
try {
	kumo_luxio_iterator* it =
		reinterpret_cast<kumo_luxio_iterator*>(iterator_data);

	Lux::IO::data_t k = {it->key->data, it->key->size};
	return it->db->del(&k);

} catch (std::exception& e) {
	set_error(e.what());
	return false;
}

#if 0
static bool kumo_luxio_iterator_del_if_older(void* iterator_data, uint64_t if_older)
try {
	kumo_luxio_iterator* it =
		reinterpret_cast<kumo_luxio_iterator*>(iterator_data);

	const char* val = (const char*)it->val->data;
	size_t vallen = it->val->size;

	if(vallen < 8 || kumo_clocktime_less(
				kumo_storage_clocktime_of(val),
				if_older)) {

		Lux::IO::data_t k = {it->key->data, it->key->size};
		return it->db->del(&k);
	}

	return false;

} catch (std::exception& e) {
	set_error(e.what());
	return false;
}
#endif


static kumo_storage_op kumo_luxio_op =
{
	kumo_luxio_create,
	kumo_luxio_free,
	kumo_luxio_open,
	kumo_luxio_close,
	kumo_luxio_get,
	kumo_luxio_set,
	kumo_luxio_update,
	NULL,
	kumo_luxio_del,
	kumo_luxio_rnum,
	kumo_luxio_backup,
	kumo_luxio_error,
	kumo_luxio_for_each,
	kumo_luxio_iterator_key,
	kumo_luxio_iterator_val,
	kumo_luxio_iterator_keylen,
	kumo_luxio_iterator_vallen,
	kumo_luxio_iterator_release_key,
	kumo_luxio_iterator_release_val,
	kumo_luxio_iterator_del,
};

extern "C"
kumo_storage_op kumo_storage_init(void)
{
	return kumo_luxio_op;
}

