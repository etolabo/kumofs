#ifndef PROTOCOL_H__
#define PROTOCOL_H__

#include "logic/hash.h"
#include "storage.h"
#include "rpc/protocol.h"
#include "rpc/message.h"

namespace kumo {


struct meta_leading_raw_ref : public msgpack::type::raw_ref {
	meta_leading_raw_ref() {}
	meta_leading_raw_ref(const char* val, size_t vallen) :
		msgpack::type::raw_ref(val, vallen) {}
};

inline meta_leading_raw_ref& operator>> (msgpack::object o, meta_leading_raw_ref& v)
{
	if(o.type != msgpack::type::RAW) { throw msgpack::type_error(); }
	v.ptr  = o.via.ref.ptr;
	v.size = o.via.ref.size;
	return v;
}

template <typename Stream>
inline msgpack::packer<Stream>& operator<< (msgpack::packer<Stream>& o, const meta_leading_raw_ref& v)
{
	static char MARGIN[DBFormat::LEADING_METADATA_SIZE] = {0};  // FIXME
	o.pack_raw(DBFormat::LEADING_METADATA_SIZE + v.size);
	o.pack_raw_body(MARGIN, DBFormat::LEADING_METADATA_SIZE);
	o.pack_raw_body(v.ptr, v.size);
	return o;
}


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
	ReplacePropose			= 66,
	ReplacePush				= 67,

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
	typedef HashSpace::Seed HSSeed;

	struct KeepAlive : define< tuple<uint32_t> > {
		KeepAlive() {}
		KeepAlive(uint32_t clock) :
			define_type(msgpack_type( clock )) {}
		uint32_t clock() const { return get<0>(); }
		// ok: UNDEFINED
	};

	struct HashSpaceRequest : define< tuple<> > {
		HashSpaceRequest() {}
		// success: hash_space:tuple<array<raw_ref>,uint64_t>
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

	struct HashSpacePush : define< tuple<HSSeed> > {
		HashSpacePush() {}
		HashSpacePush(HSSeed& hsseed) :
			define_type(msgpack_type( hsseed )) {}
		HSSeed& hsseed()				{ return get<0>(); }
		// acknowledge: true
	};

	struct ReplicateSet : define< tuple<raw_ref, raw_ref, uint32_t> > {
		ReplicateSet() {}
		ReplicateSet(
				const char* key, size_t keylen,
				const char* meta_val, size_t meta_vallen,
				uint32_t clock) :
			define_type(msgpack_type(
						raw_ref(key, keylen),
						raw_ref(meta_val, meta_vallen),
						clock
						)) {}
		const char* key() const			{ return get<0>().ptr; }
		size_t keylen() const			{ return get<0>().size; }
		const char* meta_val() const	{ return get<1>().ptr; }
		size_t meta_vallen() const		{ return get<1>().size; }
		uint32_t clock() const			{ return get<2>(); }
		// success: true
		// ignored: false
	};

	struct ReplicateDelete : define< tuple<raw_ref, uint64_t, uint32_t> > {
		ReplicateDelete() {}
		ReplicateDelete(
				const char* key, size_t keylen,
				uint64_t clocktime,
				uint32_t clock) :
			define_type(msgpack_type(
						raw_ref(key, keylen),
						clocktime,
						clock
						)) {}
		const char* key() const			{ return get<0>().ptr; }
		size_t keylen() const			{ return get<0>().size; }
		uint64_t clocktime() const		{ return get<1>(); }
		uint32_t clock() const			{ return get<2>(); }
		// success: true
		// ignored: false
	};

	struct ReplacePropose : define< tuple<raw_ref, uint64_t> > {
		ReplacePropose() {}
		ReplacePropose(const char* key, size_t keylen, uint64_t clocktime) :
			define_type(msgpack_type( raw_ref(key, keylen), clocktime )) {}
		const char* key() const			{ return get<0>().ptr; }
		size_t keylen() const			{ return get<0>().size; }
		uint64_t clocktime() const		{ return get<1>(); }
		// needed: not nil
		// not needed: nil
	};

	struct ReplacePush : define< tuple<raw_ref, raw_ref> > {
		ReplacePush() {}
		ReplacePush(
				const char* key, size_t keylen,
				const char* meta_val, size_t meta_vallen) :
			define_type(msgpack_type(
						raw_ref(key, keylen),
						raw_ref(meta_val, meta_vallen)
						)) {}
		const char* key() const			{ return get<0>().ptr; }
		size_t keylen() const			{ return get<0>().size; }
		const char* meta_val() const	{ return get<1>().ptr; }
		size_t meta_vallen() const		{ return get<1>().size; }
		// success: true
		// obsolete: nil
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

	struct Get : define< tuple<raw_ref> > {
		Get() {}
		Get(const char* key, size_t keylen) :
			define_type(msgpack_type( raw_ref(key, keylen) )) {}
		const char* key() const			{ return get<0>().ptr; }
		size_t keylen() const			{ return get<0>().size; }
		// success: tuple< data:raw, clocktime:uint64 >
		// not found: nil
	};

	struct Set : define< tuple<raw_ref, meta_leading_raw_ref> > {
		Set() {}
		Set(	const char* key, size_t keylen,
				const char* val, size_t vallen) :
			define_type(msgpack_type(
						raw_ref(key, keylen),
						meta_leading_raw_ref(val, vallen)
						)) {}
		const char* key() const			{ return get<0>().ptr; }
		size_t keylen() const			{ return get<0>().size; }
		const char* meta_val() const	{ return get<1>().ptr; }
		size_t meta_vallen() const		{ return get<1>().size; }
		// success: tuple< clocktime:uint64 >
		// failed:  nil
	};

	struct Delete : define< tuple<raw_ref> > {
		Delete() {}
		Delete(const char* key, size_t keylen) :
			define_type(msgpack_type( raw_ref(key, keylen) )) {}
		const char* key() const			{ return get<0>().ptr; }
		size_t keylen() const			{ return get<0>().size; }
		// success: true
		// not foud: false
		// failed: nil
	};

}  // namespace type

}  // namespace protocol

}  // namespace kumo

#endif /* protocol.h */

