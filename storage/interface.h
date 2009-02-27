#ifndef STORAGE_INTERFACE_H__
#define STORAGE_INTERFACE_H__

#include "storage.h"
#include "logic/clock.h"
#include <mp/pthread.h>
#include <mp/shared_buffer.h>
#include <stdint.h>
#include <msgpack.hpp>
#include <arpa/inet.h>

#include <tchdb.h>


namespace kumo {


class Storage {
public:
	//Storage(int* argc, char** argv);
	Storage(const char* path);
	~Storage();

	static const size_t KEY_META_SIZE = 8;
	static const size_t VALUE_META_SIZE = 16;


	static ClockTime clocktime_of(const char* raw_val);

	static uint64_t meta_of(const char* raw_val);

	static uint64_t hash_of(const char* raw_key);

public:
	const char* get(
			const char* raw_key, uint32_t raw_keylen,
			uint32_t* result_raw_vallen, msgpack::zone* z);

	void set(
			const char* raw_key, uint32_t raw_keylen,
			const char* raw_val, uint32_t raw_vallen);

	bool update(
			const char* raw_key, uint32_t raw_keylen,
			const char* raw_val, uint32_t raw_vallen);

	bool del(
			const char* raw_key, uint32_t raw_keylen,
			ClockTime clocktime);

	// FIXME
	//void updatev()

	uint64_t rnum();

	void backup(const char* dstpath);

	std::string error();

	template <typename F>
	void for_each(F f);

	struct iterator {
	public:
		iterator(kumo_storage_op* op, void* data);
		~iterator();

	public:
		const char* key();
		const char* val();
		size_t keylen();
		size_t vallen();
		void release_key(msgpack::zone* z);
		void release_val(msgpack::zone* z);
		void del();
		bool del_if_older(ClockTime if_older);

	private:
		void* m_data;
		kumo_storage_op* m_op;
	};

private:
	void* m_data;
	kumo_storage_op m_op;

private:
	template <typename F>
	struct for_each_data {
		kumo_storage_op* op;
		F* func;
	};

	template <typename F>
	static int for_each_callback(void* user, void* iterator_data);
};


inline ClockTime Storage::clocktime_of(const char* raw_val)
{
	return ClockTime( kumo_storage_clocktime_of(raw_val) );
}

inline uint64_t Storage::meta_of(const char* raw_val)
{
	return kumo_be64(*(uint64_t*)(raw_val+8));
}

inline uint64_t Storage::hash_of(const char* raw_key)
{
	return kumo_storage_hash_of(raw_key);
}


//Storage::Storage(int* argc, char** argv)
inline Storage::Storage(const char* path)
{
	m_op = kumo_storage_init();

	m_data = m_op.create();
	if(!m_data) {
		throw std::runtime_error("failed to initialize storage module");
	}

	//if(!m_op.open(m_data, argc, argv)) {
	if(!m_op.open(m_data, path)) {
		std::string msg = error();
		m_op.free(m_data);
		throw std::runtime_error(msg);
	}
}

inline Storage::~Storage()
{
	m_op.close(m_data);
	m_op.free(m_data);
}


inline const char* Storage::get(
		const char* raw_key, uint32_t raw_keylen,
		uint32_t* result_raw_vallen, msgpack::zone* z)
{
	return m_op.get(m_data,
			raw_key, raw_keylen,
			result_raw_vallen,
			z);
}

inline void Storage::set(
		const char* raw_key, uint32_t raw_keylen,
		const char* raw_val, uint32_t raw_vallen)
{
	if(!m_op.set(m_data,
			raw_key, raw_keylen,
			raw_val, raw_vallen)) {
		throw std::runtime_error("set failed");
	}
}

inline bool Storage::update(
		const char* raw_key, uint32_t raw_keylen,
		const char* raw_val, uint32_t raw_vallen)
{
	return m_op.update(m_data,
			raw_key, raw_keylen,
			raw_val, raw_vallen);
}

// FIXME
//void Storage::updatev()

inline bool Storage::del(
		const char* raw_key, uint32_t raw_keylen,
		ClockTime clocktime)
{
	return m_op.del(m_data,
			raw_key, raw_keylen,
			clocktime.get());
}

inline uint64_t Storage::rnum()
{
	return m_op.rnum(m_data);
}

inline void Storage::backup(const char* dstpath)
{
	if(!m_op.backup(m_data, dstpath)) {
		throw std::runtime_error("backup failed");
	}
}

inline std::string Storage::error()
{
	return std::string( m_op.error(m_data) );
}

template <typename F>
inline void Storage::for_each(F f)
{
	for_each_data<F> user = {&m_op, &f};
	int ret = m_op.for_each(m_data,
			reinterpret_cast<void*>(&user),
			&Storage::for_each_callback<F>);
	if(ret < 0) {
		throw std::runtime_error("error while iterating database");
	}
}

template <typename F>
int Storage::for_each_callback(void* user, void* iterator_data)
{
	iterator it(reinterpret_cast<for_each_data<F>*>(user)->op, iterator_data);
	try {
		(*reinterpret_cast<for_each_data<F>*>(user)->func)(it);
		return 0;
	} catch (...) {
		// FIXME log
		return -1;
	}
}


inline Storage::iterator::iterator(kumo_storage_op* op, void* data) :
	m_data(data), m_op(op) { }

inline Storage::iterator::~iterator() { }

inline const char* Storage::iterator::key()
{
	return m_op->iterator_key(m_data);
}

inline const char* Storage::iterator::val()
{
	return m_op->iterator_val(m_data);
}

inline size_t Storage::iterator::keylen()
{
	return m_op->iterator_keylen(m_data);
}

inline size_t Storage::iterator::vallen()
{
	return m_op->iterator_vallen(m_data);
}

inline void Storage::iterator::release_key(msgpack::zone* z)
{
	if(!m_op->iterator_release_key(m_data, z)) {
		throw std::bad_alloc();
	}
}

inline void Storage::iterator::release_val(msgpack::zone* z)
{
	if(!m_op->iterator_release_val(m_data, z)) {
		throw std::bad_alloc();
	}
}

inline void Storage::iterator::del()
{
	m_op->iterator_delete(m_data);
}

inline bool Storage::iterator::del_if_older(ClockTime if_older)
{
	return m_op->iterator_delete_if_older(m_data, if_older.get());
}


}  // namespace kumo

#endif /* storage/storage.h */

