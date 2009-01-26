#ifndef PROTOCOL_H__
#define PROTOCOL_H__

#include "logic/hash.h"
#include "storage.h"
#include "rpc/protocol.h"
#include "rpc/message.h"

namespace kumo {

namespace protocol {

using namespace rpc::protocol;

const static rpc::role_type MANAGER = 0;
const static rpc::role_type SERVER  = 1;


enum message_type {
	// Manager -> Server
	// Server -> Manager
	// Manager -> Manager
	KeepAlive				= 0,

	// Client -> Manager
	HashSpaceRequest		= 1,

	// Manager -> Server
	ReplaceCopyStart		= 16,
	ReplaceDeleteStart		= 17,
	CreateBackup            = 18,

	// Server -> Manager
	ReplaceCopyEnd			= 32,
	ReplaceDeleteEnd		= 33,
	WHashSpaceRequest		= 34,
	RHashSpaceRequest		= 35,

	// Manager -> Client
	HashSpacePush			= 48,

	// Server -> Server
	ReplicateSet			= 64,
	ReplicateDelete			= 65,
	ReplaceOffer			= 66,

	// Manager -> Manager
	ReplaceElection			= 80,

	// Manager -> Manager
	// Manager -> Server
	HashSpaceSync			= 81,

	// Client -> Server
	Get						= 96,
	Set						= 97,
	Delete					= 98,
};


namespace type {
	using msgpack::define;
	using msgpack::type::tuple;
	using msgpack::type::raw_ref;
	using msgpack::type_error;
	typedef HashSpace::Seed HSSeed;


	struct DBKey {
		DBKey() {}

		DBKey(const char* key, size_t keylen, uint64_t hash) :
			m_keylen(keylen), m_key(key), m_hash(hash) {}

		DBKey(const char* raw_key, size_t raw_keylen)
			{ msgpack_unpack(raw_ref(raw_key, raw_keylen)); }

		const char* data() const		{ return m_key; }
		size_t size() const				{ return m_keylen; }
		uint64_t hash() const			{ return m_hash; }

		// these functions are available only when deserialized
		const char* raw_data() const	{ return m_key - 8; }
		size_t raw_size() const			{ return m_keylen + 8; }

		template <typename Packer>
		void msgpack_pack(Packer& pk) const
		{
			uint64_t hash_be = kumo_be64(m_hash);
			pk.pack_raw(m_keylen+8);
			pk.pack_raw_body((const char*)&hash_be, 8);
			pk.pack_raw_body(m_key, m_keylen);
		}

		void msgpack_unpack(raw_ref o)
		{
			if(o.size < 8) { throw type_error(); }
			m_keylen = o.size - 8;
			m_hash = kumo_be64(*(uint64_t*)o.ptr);
			m_key = o.ptr + 8;
		}

	private:
		size_t m_keylen;
		const char* m_key;
		uint64_t m_hash;
	};

	struct DBValue {
		DBValue() {}

		DBValue(const char* val, size_t vallen, uint64_t meta) :
			m_vallen(vallen), m_val(val), m_clocktime(0), m_meta(meta) {}

		DBValue(const char* raw_val, size_t raw_vallen)
			{ msgpack_unpack(raw_ref(raw_val, raw_vallen)); }

		const char* data() const		{ return m_val; }
		size_t size() const				{ return m_vallen; }
		uint64_t clocktime() const		{ return m_clocktime; }
		uint64_t meta() const			{ return m_meta; }

		// these functions are available only when deserialized
		const char* raw_data() const	{ return m_val - 16; }
		size_t raw_size() const			{ return m_vallen + 16; }
		void raw_set_clocktime(uint64_t clocktime)
		{
			m_clocktime = clocktime;
			*((uint64_t*)raw_data()) = kumo_be64(clocktime);
		}

		template <typename Packer>
		void msgpack_pack(Packer& pk) const
		{
			uint64_t meta_be = kumo_be64(m_meta);
			uint64_t clocktime = kumo_be64(m_clocktime);
			pk.pack_raw(m_vallen + 16);
			pk.pack_raw_body((const char*)&clocktime, 8);
			pk.pack_raw_body((const char*)&meta_be, 8);
			pk.pack_raw_body(m_val, m_vallen);
		}

		void msgpack_unpack(raw_ref o)
		{
			if(o.size < 16) { throw type_error(); }
			m_vallen = o.size - 16;
			m_clocktime = kumo_be64(*(uint64_t*)o.ptr);
			m_meta = kumo_be64(*(uint64_t*)(o.ptr+8));
			m_val = o.ptr + 16;
		}

	private:
		size_t m_vallen;
		const char* m_val;
		uint64_t m_clocktime;
		uint64_t m_meta;
	};


	struct KeepAlive : define< tuple<uint32_t> > {
		KeepAlive() {}
		KeepAlive(uint32_t clock) :
			define_type(msgpack_type( clock )) {}
		uint32_t clock() const { return get<0>(); }
		// ok: UNDEFINED
	};

	struct HashSpaceRequest : define< tuple<> > {
		HashSpaceRequest() {}
		// success: HashSpacePush
	};

	struct WHashSpaceRequest : define< tuple<> > {
		WHashSpaceRequest() {}
		// success: hash_space:tuple<array<raw_ref>,uint64_t>
	};

	struct RHashSpaceRequest : define< tuple<> > {
		RHashSpaceRequest() {}
		// success: hash_space:tuple<array<raw_ref>,uint64_t>
	};

	struct ReplaceCopyStart : define< tuple<HSSeed, uint32_t> > {
		ReplaceCopyStart() {}
		ReplaceCopyStart(HSSeed& hsseed, uint32_t clock) :
			define_type(msgpack_type( hsseed, clock )) {}
		HSSeed& hsseed()				{ return get<0>(); }
		uint32_t clock() const			{ return get<1>(); }
		// accepted: true
	};

	struct ReplaceDeleteStart : define< tuple<HSSeed, uint32_t> > {
		ReplaceDeleteStart() {}
		ReplaceDeleteStart(HSSeed& hsseed, uint32_t clock) :
			define_type(msgpack_type( hsseed, clock )) {}
		HSSeed& hsseed()				{ return get<0>(); }
		uint32_t clock() const			{ return get<1>(); }
		// accepted: true
	};

	struct CreateBackup : define< tuple<std::string> > {
		CreateBackup() {}
		CreateBackup(const std::string& postfix) :
			define_type(msgpack_type( postfix )) {}
		const std::string& suffix() const { return get<0>(); }
		// success: true
	};

	struct ReplaceCopyEnd : define< tuple<uint64_t, uint32_t> > {
		ReplaceCopyEnd() {}
		ReplaceCopyEnd(uint64_t clocktime, uint32_t clock) :
			define_type(msgpack_type( clocktime, clock )) {}
		uint64_t clocktime() const		{ return get<0>(); }
		uint32_t clock() const			{ return get<1>(); }
		// acknowledge: true
	};

	struct ReplaceDeleteEnd : define< tuple<uint64_t, uint32_t> > {
		ReplaceDeleteEnd() {}
		ReplaceDeleteEnd(uint64_t clocktime, uint32_t clock) :
			define_type(msgpack_type( clocktime, clock )) {}
		uint64_t clocktime() const		{ return get<0>(); }
		uint32_t clock() const			{ return get<1>(); }
		// acknowledge: true
	};

	struct HashSpacePush : define< tuple<HSSeed, HSSeed> > {
		HashSpacePush() {}
		HashSpacePush(HSSeed& wseed, HSSeed& rseed) :
			define_type(msgpack_type( wseed, rseed )) {}
		HSSeed& wseed()					{ return get<0>(); }
		HSSeed& rseed()					{ return get<1>(); }
		// acknowledge: true
	};


	struct ReplicateSet : define< tuple<raw_ref, raw_ref, uint32_t> > {
		ReplicateSet() {}
		ReplicateSet(
				const char* raw_key, size_t raw_keylen,
				const char* raw_val, size_t raw_vallen,
				uint32_t clock) :
			define_type(msgpack_type(
						raw_ref(raw_key, raw_keylen),
						raw_ref(raw_val, raw_vallen),
						clock
						)) {}
		DBKey dbkey() const
			{ return DBKey(get<0>().ptr, get<0>().size); }
		DBValue dbval() const
			{ return DBValue(get<1>().ptr, get<1>().size); }
		uint32_t clock() const			{ return get<2>(); }
		// success: true
		// ignored: false
	};

	struct ReplicateDelete : define< tuple<raw_ref, uint64_t, uint32_t> > {
		ReplicateDelete() {}
		ReplicateDelete(
				const char* raw_key, size_t raw_keylen,
				uint64_t clocktime,
				uint32_t clock) :
			define_type(msgpack_type(
						raw_ref(raw_key, raw_keylen),
						clocktime,
						clock
						)) {}
		DBKey dbkey() const
			{ return DBKey(get<0>().ptr, get<0>().size); }
		uint64_t clocktime() const		{ return get<1>(); }
		uint32_t clock() const			{ return get<2>(); }
		// success: true
		// ignored: false
	};

	struct ReplaceOffer : define< tuple<uint16_t> > {
		ReplaceOffer() { }
		ReplaceOffer(uint16_t port) :
			define_type(msgpack_type( port )) { }
		uint16_t port() const { return get<0>(); }
		// no response
	};

	struct ReplaceElection : define< tuple<HSSeed, uint32_t> > {
		ReplaceElection() {}
		ReplaceElection(HSSeed& hsseed, uint32_t clock) :
			define_type(msgpack_type( hsseed, clock )) {}
		HSSeed& hsseed()				{ return get<0>(); }
		uint32_t clock() const			{ return get<1>(); }
		// sender   of ReplaceElection is responsible for replacing: true
		// receiver of ReplaceElection is responsible for replacing: nil
	};

	struct HashSpaceSync : define< tuple<HSSeed, HSSeed, uint32_t> > {
		HashSpaceSync() {}
		HashSpaceSync(HSSeed& wseed, HSSeed& rseed, uint32_t clock) :
			define_type(msgpack_type( wseed, rseed, clock )) {}
		HSSeed& wseed()					{ return get<0>(); }
		HSSeed& rseed()					{ return get<1>(); }
		uint32_t clock() const			{ return get<2>(); }
		// success: true
		// obsolete: nil
	};

	struct Get : define< tuple<DBKey> > {
		Get() {}
		Get(DBKey k) : define_type(msgpack_type( k )) {}
		const DBKey& dbkey() const		{ return get<0>(); }
		// success: value:DBValue
		// not found: nil
	};

	struct Set : define< tuple<uint32_t, DBKey, DBValue> > {
		Set() {}
		Set(DBKey k, DBValue v, bool async) :
			define_type(msgpack_type( (async ? 0x1 : 0), k, v )) {}
		const DBKey& dbkey() const		{ return get<1>(); }
		const DBValue& dbval() const	{ return get<2>(); }
		bool is_async() const			{ return get<0>() & 0x01; }
		// success: tuple< clocktime:uint64 >
		// failed:  nil
	};

	struct Delete : define< tuple<DBKey, uint32_t> > {
		Delete() {}
		Delete(DBKey k, bool async) :
			define_type(msgpack_type( k, (async ? 0x1 : 0) )) {}
		const DBKey& dbkey() const		{ return get<0>(); }
		bool is_async() const			{ return get<1>() & 0x01; }
		// success: true
		// not foud: false
		// failed: nil
	};


}  // namespace type

}  // namespace protocol

}  // namespace kumo

#endif /* protocol.h */

