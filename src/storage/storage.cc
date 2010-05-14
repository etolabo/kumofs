//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
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
#include "storage/storage.h"
#include "log/mlogger.h"

namespace kumo {


Storage::Storage(const char* path,
		uint32_t garbage_min_time,
		uint32_t garbage_max_time,
		size_t garbage_mem_limit) :
	m_garbage_min_time(garbage_min_time),
	m_garbage_max_time(garbage_max_time),
	m_garbage_mem_limit(garbage_mem_limit)
{
	m_op = kumo_storage_init();

	m_data = m_op.create();
	if(!m_data) {
		throw storage_init_error("failed to initialize storage module");
	}

	if(!m_op.open(m_data, path)) {
		std::string msg = error();
		m_op.free(m_data);
		throw storage_init_error(msg);
	}
}

Storage::~Storage()
{
	m_op.close(m_data);
	m_op.free(m_data);
}


void Storage::set(
		const char* raw_key, uint32_t raw_keylen,
		const char* raw_val, uint32_t raw_vallen)
{
	if(!m_op.set(m_data,
			raw_key, raw_keylen,
			raw_val, raw_vallen)) {
		throw storage_error("set failed");
	}
}


static bool storage_updateproc(void* casdata,
		const char* oldval, size_t oldvallen)
{
	if(oldvallen < Storage::VALUE_CLOCKTIME_SIZE) {
		return true;
	}

	ClockTime update_clocktime =
		ClockTime( *reinterpret_cast<uint64_t*>(casdata) );

	ClockTime old_clocktime = ClockTime( Storage::clocktime_of(oldval) );

	return old_clocktime < update_clocktime;
}


bool Storage::update(
		const char* raw_key, uint32_t raw_keylen,
		const char* raw_val, uint32_t raw_vallen)
{
	ClockTime update_clocktime = clocktime_of(raw_val);

	return m_op.update(m_data,
			raw_key, raw_keylen,
			raw_val, raw_vallen,
			&storage_updateproc,
			reinterpret_cast<void*>(&update_clocktime));
}


static bool storage_casproc(void* casdata,
		const char* oldval, size_t oldvallen)
{
	if(oldvallen < Storage::VALUE_CLOCKTIME_SIZE) {
		return false;
	}

	ClockTime compare =
		ClockTime( *reinterpret_cast<uint64_t*>(casdata) );

	ClockTime old_clocktime = ClockTime( Storage::clocktime_of(oldval) );

	return compare == old_clocktime;
}


bool Storage::cas(
		const char* raw_key, uint32_t raw_keylen,
		const char* raw_val, uint32_t raw_vallen,
		ClockTime compare)
{
	return m_op.update(m_data,
			raw_key, raw_keylen,
			raw_val, raw_vallen,
			&storage_casproc,
			static_cast<void*>(&compare));
}


namespace {
struct scoped_clock_key {
	scoped_clock_key(const char* key, uint32_t keylen, ClockTime clocktime)
	{
		m_data = (char*)::malloc(keylen+8);
		if(!m_data) {
			throw std::bad_alloc();
		}

		*(uint64_t*)m_data = clocktime.get();
		memcpy(m_data+8, key, keylen);
	}

	~scoped_clock_key()
	{
		::free(m_data);
	}

	void* data()
	{
		return m_data;
	}

	size_t size(size_t keylen)
	{
		return keylen + 8;
	}

	struct wrap {
		wrap(const char* buf, size_t buflen) :
			m_buf(buf),
			m_buflen(buflen)
		{ }
	
		const char* key() const
		{
			return m_buf + 8;
		}

		size_t keylen() const {
			return m_buflen - 8;
		}

		ClockTime clocktime() const {
			return ClockTime( *(uint64_t*)m_buf );
		}
	
	private:
		const char* m_buf;
		size_t m_buflen;
		wrap();
	};

private:
	char* m_data;
	scoped_clock_key();
	scoped_clock_key(const scoped_clock_key&);
};
}  // noname namespace


bool Storage::remove(
		const char* raw_key, uint32_t raw_keylen,
		ClockTime update_clocktime)
{
	char clockbuf[VALUE_CLOCKTIME_SIZE];
	clocktime_to(update_clocktime, clockbuf);

	bool removed = update(raw_key, raw_keylen, clockbuf, sizeof(clockbuf));
	if(!removed) {
		return false;
	}

	// push garbage

	mp::pthread_scoped_lock gclk;

	{
		scoped_clock_key clock_key(raw_key, raw_keylen, update_clocktime);

		gclk.relock(m_garbage_mutex);

		m_garbage.push(clock_key.data(), clock_key.size(raw_keylen));
	}

	while(true) {
		size_t size;
		const char* data = (const char*)m_garbage.front(&size);
		if(!data) {
			break;
		}

		scoped_clock_key::wrap garbage_key(data, size);

		if(m_garbage.total_size() > m_garbage_mem_limit) {
			// over usage over, pop garbage
			if(garbage_key.clocktime() <
					update_clocktime.before_sec(m_garbage_min_time)) {  // min check
				ClockTime ct = garbage_key.clocktime();
				m_op.del(m_data,
						garbage_key.key(), garbage_key.keylen(),
						&storage_updateproc,
						reinterpret_cast<void*>(&ct));
			}
			m_garbage.pop();

		} else if(garbage_key.clocktime() <
				update_clocktime.before_sec(m_garbage_max_time)) {  // max check
			ClockTime ct = garbage_key.clocktime();
			m_op.del(m_data,
					garbage_key.key(), garbage_key.keylen(),
					&storage_updateproc,
					reinterpret_cast<void*>(&ct));
			m_garbage.pop();

		} else {
			break;
		}
	}

	return true;

	// unlock gclk
}


namespace {
struct for_each_data {
	kumo_storage_op* op;
	void (*callback)(void* obj, Storage::iterator& it);
	void* obj;
	ClockTime clocktime_limit;
};

static int for_each_collect(void* user, void* iterator_data)
try {
	for_each_data* data = reinterpret_cast<for_each_data*>(user);

	const char* val = data->op->iterator_val(iterator_data);
	size_t vallen = data->op->iterator_vallen(iterator_data);

	if(vallen < Storage::VALUE_META_SIZE) {
		if(data->clocktime_limit.get() != 0) {  // for kumomergedb

			if(vallen < Storage::VALUE_CLOCKTIME_SIZE) {
				// invalid value
				data->op->iterator_del(iterator_data,
						&storage_updateproc,
						reinterpret_cast<void*>(&data->clocktime_limit));
	
			} else {
				// garbage
				ClockTime garbage_clocktime = Storage::clocktime_of(val);
				if(garbage_clocktime < data->clocktime_limit) {
					data->op->iterator_del(iterator_data,
						&storage_updateproc,
						reinterpret_cast<void*>(&data->clocktime_limit));
				}
			}

		}
		return 0;
	}

	Storage::iterator it(data->op, iterator_data);
	(*data->callback)(data->obj, it);

	return 0;

} catch (...) {
		return -1;
}
}  // noname namespace

void Storage::for_each_impl(void* obj, void (*callback)(void* obj, iterator& it),
		ClockTime clocktime)
{
	for_each_data data = {
		&m_op,
		callback,
		obj,
		clocktime.before_sec(m_garbage_max_time),
	};

	int ret = m_op.for_each(m_data,
			reinterpret_cast<void*>(&data), for_each_collect);

	if(ret < 0) {
		throw storage_error("error while iterating database");
	}
}


uint64_t Storage::rnum()
{
	return m_op.rnum(m_data);
}

void Storage::backup(const char* dstpath)
{
	if(!m_op.backup(m_data, dstpath)) {
		throw storage_backup_error(error());
	}
}

std::string Storage::error()
{
	return std::string( m_op.error(m_data) );
}


}  // namespace kumo

