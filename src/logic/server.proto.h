#include "server/proto.h"
#include "logic/msgtype.h"
#include "logic/cluster_logic.h"
#include <msgpack.hpp>
#include <string>
#include <stdint.h>

namespace kumo {
namespace server {


@message proto_network::KeepAlive           =   0
@message proto_network::HashSpaceSync       =   2
@message proto_replace::ReplaceCopyStart    =   8
@message proto_replace::ReplaceDeleteStart  =   9
@message proto_replace_stream::ReplaceOffer =  16
@message proto_store::ReplicateSet          =  32
@message proto_store::ReplicateDelete       =  33
@message proto_store::Get                   =  34
@message proto_store::Set                   =  35
@message proto_store::Delete                =  36
@message proto_control::CreateBackup        =  96
@message proto_control::GetStatus           =  97
@message proto_control::SetConfig           =  98


@rpc proto_network
	message KeepAlive +cluster {
		Clock adjust_clock;
		// ok: UNDEFINED
	};

	message HashSpaceSync {
		msgtype::HSSeed wseed;
		msgtype::HSSeed rseed;
		Clock adjust_clock;
		// success: true
		// obsolete: nil
	};

public:
	void keep_alive();
	void renew_w_hash_space();
	void renew_r_hash_space();

private:
	RPC_REPLY_DECL(KeepAlive, from, res, err, life);
	RPC_REPLY_DECL(WHashSpaceRequest, from, res, err, life);
	RPC_REPLY_DECL(RHashSpaceRequest, from, res, err, life);
@end


@code proto_store
struct store_flags;
typedef msgtype::flags<store_flags, 0>    store_flags_none;
typedef msgtype::flags<store_flags, 0x01> store_flags_async;
struct store_flags : public msgtype::flags_base {
	bool is_async() { return is_set<store_flags_async>(); }
};

struct replicate_flags;
typedef msgtype::flags<replicate_flags, 0>    replicate_flags_none;
typedef msgtype::flags<replicate_flags, 0x01> replicate_flags_by_rhs;
struct replicate_flags : msgtype::flags_base {
	bool is_rhs() const { return is_set<replicate_flags_by_rhs>(); }
};
@end


@rpc proto_store
	message Get {
		msgtype::DBKey dbkey;
		// success: value:DBValue
		// not found: nil
	};

	message Set {
		store_flags flags;
		msgtype::DBKey dbkey;
		msgtype::DBValue dbval;
		// success: tuple< clocktime:ClockTime >
		// failed:  nil
	};

	message Delete {
		store_flags flags;
		msgtype::DBKey dbkey;
		// success: true
		// not foud: false
		// failed: nil
	};

	message ReplicateSet {
		Clock adjust_clock;
		replicate_flags flags;
		msgtype::DBKey dbkey;
		msgtype::DBValue dbval;
		// success: true
		// ignored: false
	};

	message ReplicateDelete {
		Clock adjust_clock;
		replicate_flags flags;
		ClockTime delete_clocktime;
		msgtype::DBKey dbkey;
		// success: true
		// ignored: false
	};

private:
	void check_replicator_assign(HashSpace& hs, uint64_t h);
	void check_coordinator_assign(HashSpace& hs, uint64_t h);

	bool SetByRhsWhs(weak_responder response, auto_zone& z,
			msgtype::DBKey& key, msgtype::DBValue& val,
			bool is_async);
	void SetByWhs(weak_responder response, auto_zone& z,
			msgtype::DBKey& key, msgtype::DBValue& val,
			bool is_async);

	RPC_REPLY_DECL(ReplicateSet, from, res, err, life,
			rpc::retry<ReplicateSet>* retry,
			volatile unsigned int* copy_required,
			rpc::weak_responder response, ClockTime clocktime);

	bool DeleteByRhsWhs(weak_responder response, auto_zone& z,
			msgtype::DBKey& key,
			bool is_async);
	void DeleteByWhs(weak_responder response, auto_zone& z,
			msgtype::DBKey& key,
			bool is_async);

	RPC_REPLY_DECL(ReplicateDelete, from, res, err, life,
			rpc::retry<ReplicateDelete>* retry,
			volatile unsigned int* copy_required,
			rpc::weak_responder response, bool deleted);
@end



@rpc proto_replace
	message ReplaceCopyStart +cluster {
		msgtype::HSSeed hsseed;
		Clock adjust_clock;
		// accepted: true
	};

	message ReplaceDeleteStart +cluster {
		msgtype::HSSeed hsseed;
		Clock adjust_clock;
		// accepted: true
	};

private:
	static bool test_replicator_assign(const HashSpace& hs, uint64_t h, const address& target);

	typedef std::vector<address> addrvec_t;
	typedef addrvec_t::iterator addrvec_iterator;

	void replace_copy(const address& manager_addr, HashSpace& hs);
	struct for_each_replace_copy;
	void finish_replace_copy(ClockTime clocktime, REQUIRE_STLK);
	RPC_REPLY_DECL(ReplaceCopyEnd, from, res, err, life);

	void replace_delete(shared_node& manager, HashSpace& hs);
	struct for_each_replace_delete;
	RPC_REPLY_DECL(ReplaceDeleteEnd, from, res, err, life);

private:
	class replace_state {
	public:
		replace_state();
		~replace_state();
	public:
		void reset(const address& mgr, ClockTime ct);
		void pushed(ClockTime ct);
		void push_returned(ClockTime ct);
		const address& mgr_addr() const;
		bool is_finished(ClockTime ct) const;
		void invalidate();
	private:
		int m_push_waiting;
		ClockTime m_clocktime;
		address m_mgr;
	};

	mp::pthread_mutex m_state_mutex;
	replace_state m_state;

public:
	void replace_offer_push(ClockTime replace_time, REQUIRE_STLK);
	void replace_offer_pop(ClockTime replace_time, REQUIRE_STLK);
	mp::pthread_mutex& state_mutex() { return m_state_mutex; }
@end


@rpc proto_replace_stream
	message ReplaceOffer +cluster {
		uint16_t port;
		// no response
	};

public:
	proto_replace_stream(address stream_addr);
	~proto_replace_stream();

private:
	int m_stream_lsock;
	address m_stream_addr;

public:
	const address& stream_addr() const
	{
		return m_stream_addr;
	}

	void init_stream(int lsock);
	void run_stream();
	void stop_stream();

private:
	class stream_accumulator;
	typedef mp::shared_ptr<stream_accumulator> shared_stream_accumulator;
	typedef std::vector<shared_stream_accumulator> accum_set_t;

public:
	class offer_storage {
	public:
		offer_storage(const std::string& basename, ClockTime replace_time);
		~offer_storage();
	public:
		void add(const address& addr,
				const char* key, size_t keylen,
				const char* val, size_t vallen);
		void commit(accum_set_t* dst);
	private:
		accum_set_t m_set;
		const std::string& m_basename;
		ClockTime m_replace_time;
	private:
		offer_storage();
		offer_storage(const offer_storage&);
	};

	void send_offer(offer_storage& offer, ClockTime replace_time);

private:
	mp::pthread_mutex m_accum_set_mutex;
	accum_set_t m_accum_set;

	struct accum_set_comp;
	static accum_set_t::iterator accum_set_find(
			accum_set_t& map, const address& addr);

	RPC_REPLY_DECL(ReplaceOffer, from, res, err, life,
			address addr);

	void stream_accepted(int fd, int err);
	void stream_connected(int fd, int err);

	std::auto_ptr<mp::wavy::core> m_stream_core;
	class stream_handler;
	friend class stream_handler;
@end


@code proto_control
enum status_type {
	STAT_PID			= 0,
	STAT_UPTIME			= 1,
	STAT_TIME			= 2,
	STAT_VERSION		= 3,
	STAT_CMD_GET		= 4,
	STAT_CMD_SET		= 5,
	STAT_CMD_DELETE		= 6,
	STAT_DB_ITEMS		= 7,
	STAT_CLOCKTIME		= 8,
};
@end

@rpc proto_control
	message CreateBackup {
		std::string suffix;
		// success: true
	};

	message GetStatus {
		uint32_t command;
	};
@end


}  // namespace server
}  // namespace kumo

