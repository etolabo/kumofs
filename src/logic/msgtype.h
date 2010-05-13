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
#ifndef LOGIC_MSGTYPE_H__
#define LOGIC_MSGTYPE_H__

#include "storage/storage.h"
#include "logic/hash.h"
#include <msgpack.hpp>

namespace kumo {
namespace msgtype {

using namespace msgpack::type;
using msgpack::type_error;
typedef HashSpace::Seed HSSeed;


struct DBKey {
	DBKey() {}

	DBKey(const char* key, size_t keylen, uint64_t hash) :
		m_keylen(keylen), m_key(key), m_hash(hash) {}

	DBKey(const char* raw_key, size_t raw_keylen)
	{
		msgpack_unpack(raw_ref(raw_key, raw_keylen));
	}

	const char* data() const		{ return m_key; }
	size_t size() const				{ return m_keylen; }
	uint64_t hash() const			{ return m_hash; }

	// these functions are available only when deserialized
	const char* raw_data() const	{ return m_key - Storage::KEY_META_SIZE; }
	size_t raw_size() const			{ return m_keylen + Storage::KEY_META_SIZE; }

	template <typename Packer>
	void msgpack_pack(Packer& pk) const
	{
		char metabuf[Storage::KEY_META_SIZE];
		Storage::hash_to(m_hash, metabuf);
		pk.pack_raw(m_keylen + Storage::KEY_META_SIZE);
		pk.pack_raw_body(metabuf, Storage::KEY_META_SIZE);
		pk.pack_raw_body(m_key, m_keylen);
	}

	void msgpack_unpack(raw_ref o)
	{
		if(o.size < Storage::KEY_META_SIZE) {
			throw type_error();
		}
		m_keylen = o.size - Storage::KEY_META_SIZE;
		m_hash = Storage::hash_of(o.ptr);
		m_key = o.ptr + Storage::KEY_META_SIZE;
	}

private:
	size_t m_keylen;
	const char* m_key;
	uint64_t m_hash;
};


struct DBValue {
	DBValue() : m_clocktime(0) {}
	DBValue(ClockTime ct) : m_clocktime(ct) { }

	DBValue(const char* val, size_t vallen, uint16_t meta, uint64_t clocktime) :
		m_vallen(vallen), m_val(val), m_clocktime(clocktime), m_meta(meta) {}

	DBValue(const char* raw_val, size_t raw_vallen) :
		m_clocktime(0)
	{
		msgpack_unpack(raw_ref(raw_val, raw_vallen));
	}

	const char* data() const		{ return m_val; }
	size_t size() const				{ return m_vallen; }
	ClockTime clocktime() const		{ return m_clocktime; }
	uint16_t meta() const			{ return m_meta; }

	// these functions are available only when deserialized
	const char* raw_data() const	{ return m_val - Storage::VALUE_META_SIZE; }
	size_t raw_size() const			{ return m_vallen + Storage::VALUE_META_SIZE; }
	void raw_set_clocktime(ClockTime clocktime)
	{
		m_clocktime = clocktime;
		Storage::clocktime_to(clocktime, const_cast<char*>(raw_data()));
	}

	template <typename Packer>
	void msgpack_pack(Packer& pk) const
	{
		char metabuf[Storage::VALUE_META_SIZE];
		Storage::clocktime_to(m_clocktime, metabuf);
		Storage::meta_to(m_meta, metabuf);
		pk.pack_raw(m_vallen + Storage::VALUE_META_SIZE);
		pk.pack_raw_body(metabuf, Storage::VALUE_META_SIZE);
		pk.pack_raw_body(m_val, m_vallen);
	}

	void msgpack_unpack(raw_ref o)
	{
		if(o.size < Storage::VALUE_META_SIZE) {
			throw type_error();
		}
		m_clocktime = Storage::clocktime_of(o.ptr);
		m_meta      = Storage::meta_of(o.ptr);
		m_vallen    = o.size - Storage::VALUE_META_SIZE;
		m_val       = o.ptr  + Storage::VALUE_META_SIZE;
	}

private:
	size_t m_vallen;
	const char* m_val;
	ClockTime m_clocktime;
	uint16_t m_meta;
};


struct flags_base {
	template <typename T>
	bool is_set() const { return m & T::flag; }

protected:
	uint32_t m;
public:
	flags_base() : m(0) { }
	template <typename Packer>
	void msgpack_pack(Packer& pk) const { pk.pack(m); }
	void msgpack_unpack(uint32_t f) { m = f; }
};

template <typename Base, uint32_t Flag>
struct flags : Base {
	static const uint32_t flag = Flag;
	flags() { this->flags_base::m = flag; }
};


}  // namespace msgtype
}  // namespace kumo

#endif /* logic/msgtype.h */

