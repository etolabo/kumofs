#ifndef STORAGE_STORAGE_H__
#define STORAGE_STORAGE_H__

#include "logic/clock.h"
#include <mp/pthread.h>
#include <mp/shared_buffer.h>
#include <stdint.h>
#include <msgpack.hpp>
#include <arpa/inet.h>

#include <tchdb.h>

// Database entry format
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


class Storage {
public:
	Storage(const char* path);
	~Storage();

	static const size_t VALUE_META_SIZE = 16;
	static const size_t KEY_META_SIZE = 8;

	static ClockTime clocktime_of(const char* raw_val)
	{
		return ClockTime( kumo_be64(*(uint64_t*)raw_val) );
	}

	static uint64_t meta_of(const char* raw_val)
	{
		return kumo_be64(*(uint64_t*)(raw_val+8));
	}

	static uint64_t hash_of(const char* raw_key)
	{
		return kumo_be64(*(uint64_t*)raw_key);
	}

public:
	const char* get(const char* raw_key, uint32_t raw_keylen,
			uint32_t* result_raw_vallen, msgpack::zone& z);

	bool update(const char* raw_key, uint32_t raw_keylen,
			const char* raw_val, uint32_t raw_vallen);

	bool del(const char* raw_key, uint32_t raw_keylen,
			ClockTime ct, ClockTime* result_clocktime);

	//void updatev()

public:
	void try_flush();
	void flush();

	uint64_t rnum();
	void copy(const char* dstpath);
	std::string error();

public:
	template <typename F>
	void for_each(F f);

	struct iterator {
	public:
		iterator(TCHDB* db);
		~iterator();
	public:
		bool del(ClockTime ct);
		bool del_nocheck();
		bool next();

	public:
		const char* key()
			{ return TCXSTRPTR(m_key); }
		size_t keylen()
			{ return TCXSTRSIZE(m_key); }
		const char* val()
			{ return TCXSTRPTR(m_val); }
		size_t vallen()
			{ return TCXSTRSIZE(m_val); }
	public:
		void release_key(msgpack::zone& z)
		{
			z.push_finalizer(&finalize_xstr_del, m_key);
			m_key = NULL;
		}
		void release_val(msgpack::zone& z)
		{
			z.push_finalizer(&finalize_xstr_del, m_val);
			m_val = NULL;
		}
	private:
		TCXSTR* m_key;
		TCXSTR* m_val;
		TCHDB* m_db;
		static void finalize_xstr_del(void* xstr)
		{
			tcxstrdel(reinterpret_cast<TCXSTR*>(xstr));
		}

	private:
		iterator();
		iterator(const iterator&);
	};

private:
	struct const_db {
		const_db(TCHDB* db) : m_db(db) { }

		int get(const char* key, uint32_t keylen,
				char* valbuf, uint32_t vallen);

		bool get_clocktime(const char* key, uint32_t keylen, ClockTime* result);

		int vsiz(const char* key, uint32_t keylen);

	private:
		TCHDB* m_db;
	};

private:
	enum dirty_mode {
		CLEAN        = 0,
		DIRTY_SET    = 1,
		DIRTY_DELETE = 2,
	};

	struct entry {
		entry() :
			keylen(0), buflen(0),
			ptr(NULL), dirty(CLEAN) { }

		uint32_t keylen;
		uint32_t buflen;  // vallen = buflen - keylen
		char* ptr;
		mp::shared_buffer::reference ref;
		volatile dirty_mode dirty;

		bool key_equals(const char* raw_key, uint32_t raw_keylen)
		{
			return keylen == raw_keylen && memcmp(ptr, raw_key, raw_keylen);
		}

		ClockTime clocktime()
		{
			return clocktime_of(ptr+keylen);
		}
	};

	class slot {
	public:
		bool get(const char* raw_key, uint32_t raw_keylen,
				const_db cdb,
				const char** result_raw_val, uint32_t* result_raw_vallen,
				msgpack::zone& z);

		bool update(const char* raw_key, uint32_t raw_keylen,
				const char* raw_val, uint32_t raw_vallen,
				const_db cdb,
				bool* result_updated);

		bool del(const char* raw_key, uint32_t raw_keylen,
				ClockTime ct,
				const_db cdb,
				bool* result_deleted, ClockTime* result_clocktime);

		void flush(TCHDB* db);

	private:
		entry& entry_of(const char* raw_key);
		bool get_entry(entry& e, const_db cdb, const char* raw_key, uint32_t raw_keylen);

	private:
		mp::pthread_mutex m_mutex;
		mp::shared_buffer m_buffer;
		entry* m_entries;
		size_t m_entries_size;
	};

private:
	static uint64_t ihash_of(const char* raw_key);
	slot& slot_of(const char* raw_key, uint32_t raw_keylen);

	mp::pthread_rwlock m_global_lock;
	TCHDB* m_db;

	slot* m_slots;
	size_t m_slots_size;

	volatile bool dirty_exist;
};


inline Storage::iterator::iterator(TCHDB* db) :
	m_db(db)
{
	tchdbiterinit(m_db);
}

inline Storage::iterator::~iterator()
{
	if(m_key) { tcxstrdel(m_key); }
	if(m_val) { tcxstrdel(m_val); }
}

bool Storage::iterator::next()
{
	if(!m_key) { m_key = tcxstrnew(); }
	if(!m_val) { m_val = tcxstrnew(); }
	return tchdbiternext3(m_db, m_key, m_val);
}

template <typename F>
void Storage::for_each(F f)
{
	mp::pthread_scoped_wrlock lk(m_global_lock);

	flush();
	// FIXME clear

	iterator it(m_db);
	while(it.next()) {
		if(it.keylen() < KEY_META_SIZE || it.vallen() < VALUE_META_SIZE) {
			it.del_nocheck();
		} else {
			f(it);
		}
	}
}


}  // namespace kumo

#endif /* storage/storage.h */

