#include "storage/storage.h"

namespace kumo {


inline uint64_t Storage::ihash_of(const char* raw_key)
{
	return *(uint64_t*)raw_key;
}

inline Storage::slot& Storage::slot_of(const char* raw_key, uint32_t raw_keylen)
{
	if(raw_keylen < KEY_META_SIZE) {
		throw std::runtime_error("invalid key");
	}
	return m_slots[((uint32_t)ihash_of(raw_key)) % m_slots_size];
}

inline Storage::entry& Storage::slot::entry_of(const char* raw_key)
{
	return m_entries[((uint32_t)(ihash_of(raw_key) >> 32)) % m_entries_size];
}


inline int Storage::const_db::get(const char* key, uint32_t keylen,
		char* valbuf, uint32_t vallen)
{
	return tchdbget3(m_db, key, keylen, valbuf, vallen);
}

inline int Storage::const_db::vsiz(const char* key, uint32_t keylen)
{
	return tchdbvsiz(m_db, key, keylen);
}


const char* Storage::get(const char* raw_key, uint32_t raw_keylen,
		uint32_t* result_raw_vallen, msgpack::zone& z)
{
	while(true) {
		{
			mp::pthread_scoped_rdlock lk(m_global_lock);
			const char* val;
			if( slot_of(raw_key, raw_keylen).get(
						raw_key, raw_keylen,
						const_db(m_db),
						&val, result_raw_vallen, z) ) {
				return val;
			}
		}
		flush();
	}
}

bool Storage::update(const char* raw_key, uint32_t raw_keylen,
		const char* raw_val, uint32_t raw_vallen)
{
	if(raw_vallen < VALUE_META_SIZE) {
		throw std::runtime_error("bad data");
	}

	while(true) {
		{
			mp::pthread_scoped_rdlock lk(m_global_lock);
			bool updated;
			if( slot_of(raw_key, raw_keylen).update(
						raw_key, raw_keylen,
						raw_val, raw_vallen,
						const_db(m_db),
						&updated) ) {
				if(updated) {
					dirty_exist = true;
				}
				return updated;
			}
		}
		flush();
	}
}

bool Storage::del(const char* raw_key, uint32_t raw_keylen)
{
	while(true) {
		{
			mp::pthread_scoped_rdlock lk(m_global_lock);
			bool deleted;
			if( slot_of(raw_key, raw_keylen).del(
						raw_key, raw_keylen,
						const_db(m_db),
						&deleted) ) {
				if(deleted) {
					dirty_exist = true;
				}
				return deleted;
			}
		}
		flush();
	}
}


bool Storage::slot::get(const char* raw_key, uint32_t raw_keylen,
		const_db cdb,
		const char** result_raw_val, uint32_t* result_raw_vallen,
		msgpack::zone& z)
{
	mp::pthread_scoped_lock lk(m_mutex);

	entry& e( entry_of(raw_key) );

	if(e.key_equals(raw_key, raw_keylen)) {
		if(e.dirty == DIRTY_DELETE) {
			*result_raw_val = NULL;
			*result_raw_vallen = 0;
			return true;
		}

	} else {

		if(e.dirty != CLEAN) { return false; }

		if( !get_entry(e, cdb, raw_key, raw_keylen) ) {
			// not found
			*result_raw_val = NULL;
			*result_raw_vallen = 0;
			return true;
		}
	}

	z.allocate<mp::shared_buffer::reference>(e.ref);
	*result_raw_val = e.ptr + e.keylen;
	*result_raw_vallen = e.buflen - e.keylen;

	return true;
}


bool Storage::slot::update(const char* raw_key, uint32_t raw_keylen,
		const char* raw_val, uint32_t raw_vallen,
		const_db cdb,
		bool* result_updated)
{
	mp::pthread_scoped_lock lk(m_mutex);

	entry& e( entry_of(raw_key) );

	if(e.key_equals(raw_key, raw_keylen)) {
		if(clocktime_of(raw_val) < e.clocktime()) {
			*result_updated = false;
			return true;
		}

	} else {

		ClockTime ct(0);
		if(get_clocktime(cdb, raw_key, raw_keylen, &ct)) {
			if(clocktime_of(raw_val) < ct) {
				*result_updated = false;
				return true;
			}
		}

		if(e.dirty != CLEAN) { return false; }

		if(e.buflen < raw_keylen + raw_vallen) {
			e.ptr = (char*)m_buffer.allocate(raw_keylen + raw_vallen, &e.ref);
		}

		e.keylen = raw_keylen;
		memcpy(e.ptr, raw_key, raw_keylen);
	}

	e.buflen = e.keylen + raw_vallen;
	memcpy(e.ptr + e.keylen, raw_val, raw_vallen);

	e.dirty = DIRTY_SET;
	*result_updated = true;

	return true;
}

bool Storage::slot::del(const char* raw_key, uint32_t raw_keylen,
		const_db cdb,
		bool* result_deleted)
{
	mp::pthread_scoped_lock lk(m_mutex);

	entry& e( entry_of(raw_key) );

	if(e.key_equals(raw_key, raw_keylen)) {
		if(e.dirty == DIRTY_DELETE) {
			*result_deleted = false;
			return true;
		}

	} else {

		if(e.dirty != CLEAN) { return false; }

		if( !get_entry(e, cdb, raw_key, raw_keylen) ) {
			// not found
			*result_deleted = false;
			return true;
		}
	}

	if(clocktime_of(e.ptr+e.keylen) < e.clocktime()) {
		*result_deleted = false;
		return true;
	}

	e.dirty = DIRTY_DELETE;
	*result_deleted = true;

	return true;
}


inline bool Storage::slot::get_clocktime(const_db cdb, const char* key, uint32_t keylen, ClockTime* result)
{
	char buf[8];
	int len = cdb.get(key, keylen, buf, sizeof(buf));
	if(len < (int32_t)sizeof(buf)) { return false; }
	*result = ClockTime( kumo_be64(*(uint64_t*)buf) );
	return true;
}

bool Storage::slot::get_entry(entry& e, const_db cdb, const char* raw_key, uint32_t raw_keylen)
{
	m_buffer.reserve(1024);  // FIXME

	char* buf;
	int sz;

	retry:
	{
		buf = (char*)m_buffer.buffer();

		sz = cdb.get(raw_key, raw_keylen,
				buf, m_buffer.buffer_capacity() - raw_keylen);

		if(sz < (int)VALUE_META_SIZE) {
			// not found
			return false;
		}

		if((size_t)sz == m_buffer.buffer_capacity() - raw_keylen) {
			// insufficient buffer
			sz = cdb.vsiz(raw_key, raw_keylen);
			if(sz < 0) {
				// not found
				return false;
			}
			m_buffer.reserve(sz);
			goto retry;
		}
	}

	e.ptr = buf;
	e.keylen = raw_keylen;
	e.buflen = raw_keylen + sz;

	memcpy(e.ptr, raw_key, raw_keylen);

	m_buffer.allocate(e.buflen, &e.ref);

	return true;
}
	

void Storage::try_flush()
{
	if(!dirty_exist) { return; }
	flush();
}

void Storage::flush()
{
	mp::pthread_scoped_wrlock lk(m_global_lock);
	for(slot* s=m_slots, * const send(m_slots+m_slots_size);
			s != send; ++s) {
		s->flush(m_db);
	}
	dirty_exist = false;
}

inline void Storage::slot::flush(TCHDB* db)
{
	for(entry* e=m_entries, * const eend(m_entries+m_entries_size);
			e != eend; ++e) {
		switch(e->dirty) {
		case CLEAN:
			// do nothing
			break;

		case DIRTY_SET:
			// FIXME error handling
			tchdbput(db, e->ptr, e->keylen, e->ptr+e->keylen, e->buflen-e->keylen);
			e->dirty = CLEAN;
			break;

		case DIRTY_DELETE:
			// FIXME error handling
			tchdbout(db, e->ptr, e->keylen);
			e->dirty = CLEAN;
			break;
		}
	}
}


}  // namespace kumo

