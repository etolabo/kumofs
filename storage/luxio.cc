// FIXME __STDC_LIMIT_MACROS
#include <luxio/btree.h>
#include "storage.h"
#include "common.h"
#include <algorithm>

static Lux::IO::Btree* s_luxbt = NULL;

void Storage::finalize_clean_data(void* val)
{
	s_luxbt->clean_data(
			reinterpret_cast<Lux::IO::data_t*>(val));
}

Storage::Storage(const char* path)
{
	if(s_luxbt) {
		throw std::logic_error("can't create multiple luxio db instance");
	}
	s_luxbt = new Lux::IO::Btree(Lux::IO::CLUSTER);
	//s_luxbt->set_lock_type(Lux::IO::LOCK_THREAD);
	s_luxbt->set_lock_type(Lux::IO::LOCK_PROCESS);
	if(!s_luxbt->open(path, Lux::DB_CREAT)) {
		delete s_luxbt;
		s_luxbt = NULL;
		throw std::runtime_error("can't open luxio db");
	}
}

Storage::~Storage()
{
	s_luxbt->close();
	delete s_luxbt;
	s_luxbt = NULL;
}

const char* Storage::get(const char* key, uint32_t keylen,
		uint32_t* result_vallen,
		mp::zone& z)
{
	/*
	Lux::IO::data_t* v = s_luxbt->get(key, keylen, Lux::IO::SYSTEM);
	if(v == NULL) { return NULL; }
	try {
		z.push_finalizer(&Storage::finalize_clean_data, v);
	} catch (...) {
		s_luxbt->clean_data(v);
		throw;
	}
	*result_vallen = v->size;
	return (const char*)v->data;
	*/
	LOG_ERROR("db ",(void*)s_luxbt);
	LOG_ERROR("get ",keylen," ",key);
	Lux::IO::data_t k = {key, keylen};
	Lux::IO::data_t* v = NULL;
	if(!s_luxbt->get(&k, &v, Lux::IO::SYSTEM) || v == NULL) {
		LOG_WARN("get data failed ",(void*)v);
		return NULL;
	}
	LOG_WARN("get data ",(void*)v);
	try {
		z.push_finalizer(&Storage::finalize_clean_data, v);
	} catch (...) {
		s_luxbt->clean_data(v);
		throw;
	}
	*result_vallen = v->size;
	return (const char*)v->data;
}

int32_t Storage::get_header(const char* key, uint32_t keylen,
		char* valbuf, uint32_t vallen)
{
	// FIXME
	/*
	Lux::IO::data_t* v = s_luxbt->get(key, keylen);
	if(v == NULL) { return 0; }
	uint32_t len = std::min(v->size, vallen);
	memcpy(valbuf, v->data, len);
	s_luxbt->clean_data(v);
	return len;
	*/
	Lux::IO::data_t k = {key, keylen};
	Lux::IO::data_t* v = NULL;
	if(!s_luxbt->get(&k, &v, Lux::IO::SYSTEM) || v == NULL) {
		return -1;
	}
	uint32_t len = std::min(v->size, vallen);
	memcpy(valbuf, v->data, len);
	s_luxbt->clean_data(v);
	return len;
}

void Storage::set(const char* key, uint32_t keylen,
		const char* val, uint32_t vallen)
{
	LOG_ERROR("db ",(void*)s_luxbt);
	LOG_ERROR("set ",keylen," ",key);
	if(!s_luxbt->put(key, keylen, val, vallen)) {
		throw std::runtime_error("store failed");
	}
}

bool Storage::del(const char* key, uint32_t keylen)
{
	return s_luxbt->del(key, keylen);
}

void Storage::iterator_init(iterator& it)
{
	it.reset();
}

bool Storage::iterator_next(iterator& it)
{
	return it.next();
}

void Storage::close()
{
	s_luxbt->close();
}


#define AS_DATA(d) reinterpret_cast<Lux::IO::data_t*>(d)
#define AS_DATA_P(d) reinterpret_cast<Lux::IO::data_t**>(d)
#define AS_CURSOR(c) reinterpret_cast<Lux::IO::cursor_t*>(c)

Storage::iterator::iterator() :
	m_cursor(NULL), m_key(NULL), m_val(NULL) {}

Storage::iterator::~iterator()
{
	clear();
}

const char* Storage::iterator::key()
{
	return (const char*)AS_DATA(m_key)->data;
}

size_t Storage::iterator::keylen()
{
	return AS_DATA(m_key)->size;
}
const char* Storage::iterator::val()
{
	return (const char*)AS_DATA(m_val)->data;
}
size_t Storage::iterator::vallen()
{
	return AS_DATA(m_val)->size;
}

void Storage::iterator::release_key(mp::zone& z)
{
	z.push_finalizer(&Storage::finalize_clean_data, m_key);
	m_key = NULL;
}

void Storage::iterator::release_val(mp::zone& z)
{
	z.push_finalizer(&Storage::finalize_clean_data, m_val);
	m_val = NULL;
}

void Storage::iterator::reset()
{
	clear();
	m_cursor = (void*)s_luxbt->cursor_init();
//	if(!s_luxbt->first(AS_CURSOR(m_cursor))) {
//		throw std::runtime_error("can't initialize luxio cursor");
//	}
}

bool Storage::iterator::next()
{
	if(s_luxbt->next(AS_CURSOR(m_cursor))) {
		return s_luxbt->cursor_get(
				AS_CURSOR(m_cursor),
				AS_DATA_P(&m_key), AS_DATA_P(&m_val),
				Lux::IO::SYSTEM);
	} else {
		return false;
	}
}

void Storage::iterator::clear()
{
	if(m_key) {
		s_luxbt->clean_data(AS_DATA(m_key));
		m_key = NULL;
	}
	if(m_val) {
		s_luxbt->clean_data(AS_DATA(m_val));
		m_val = NULL;
	}
	if(m_cursor) {
		s_luxbt->cursor_fin(AS_CURSOR(m_cursor));
		m_cursor = NULL;
	}
}

