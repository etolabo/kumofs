#include "manager/proto.h"
#include "logic/msgtype.h"
#include "logic/cluster_logic.h"
#include <msgpack.hpp>
#include <string>
#include <stdint.h>

namespace kumo {
namespace manager {


@message proto_network::KeepAlive           =   0
@message proto_network::HashSpaceRequest    =   1
@message proto_network::HashSpaceSync       =   2
@message proto_network::WHashSpaceRequest   =   4
@message proto_network::RHashSpaceRequest   =   5
@message proto_replace::ReplaceCopyEnd      =  10
@message proto_replace::ReplaceDeleteEnd    =  11
@message proto_replace::ReplaceElection     =  12
@message proto_control::GetNodesInfo        =  99
@message proto_control::AttachNewServers    = 100
@message proto_control::DetachFaultServers  = 101
@message proto_control::CreateBackup        = 102
@message proto_control::SetAutoReplace      = 103
@message proto_control::StartReplace        = 104


@rpc proto_network
	message KeepAlive.1 +cluster {
		Clock clock;
		// ok: UNDEFINED
	};

	message HashSpaceRequest.1 {
		// success: gateway::proto_network::HashSpacePush_1
	};

	message WHashSpaceRequest.1 {
		// success: hash_space:tuple<array<raw_ref>,uint64_t>
	};

	message RHashSpaceRequest.1 {
		// success: hash_space:tuple<array<raw_ref>,uint64_t>
	};

	message HashSpaceSync.1 +cluster {
		msgtype::HSSeed wseed;
		msgtype::HSSeed rseed;
		Clock clock;
		// success: true
		// obsolete: nil
	};

public:
	void keep_alive();

	void sync_hash_space_servers(REQUIRE_HSLK, REQUIRE_SSLK);
	void sync_hash_space_partner(REQUIRE_HSLK);
	void push_hash_space_clients(REQUIRE_HSLK);

private:
	RPC_REPLY_DECL(KeepAlive_1, from, res, err, life);

	RPC_REPLY_DECL(HashSpaceSync_1, from, res, err, life);
	RPC_REPLY_DECL(HashSpacePush_1, from, res, err, life);
@end


@code proto_replace
class framework;
@end

@rpc proto_replace
	message ReplaceCopyEnd.1 {
		ClockTime clocktime;  // FIXME ClockTime?
		Clock clock;
		// acknowledge: true
	};

	message ReplaceDeleteEnd.1 {
		ClockTime clocktime;  // FIXME ClockTime?
		Clock clock;
		// acknowledge: true
	};

	message ReplaceElection.1 +cluster {
		msgtype::HSSeed hsseed;
		Clock clock;
		// sender   of ReplaceElection is responsible for replacing: true
		// receiver of ReplaceElection is responsible for replacing: nil
	};

public:
	proto_replace();
	~proto_replace();

public:
	void attach_new_servers(REQUIRE_HSLK);
	void detach_fault_servers(REQUIRE_HSLK);

	void start_replace(REQUIRE_HSLK);

	void add_server(const address& addr, shared_node& s);
	void remove_server(const address& addr);

private:
	void replace_election();
	void delayed_replace_election();
	void cas_checked_replace_election(int cas);
	int m_delayed_replace_cas;

	RPC_REPLY_DECL(ReplaceElection_1, from, res, err, life);
	RPC_REPLY_DECL(ReplaceCopyStart_1, from, res, err, life);
	RPC_REPLY_DECL(ReplaceDeleteStart_1, from, res, err, life);

	void finish_replace_copy(REQUIRE_RELK);
	void finish_replace(REQUIRE_RELK);

private:
	class ReplaceContext {
	public:
		ReplaceContext();
		~ReplaceContext();
	public:
		ClockTime clocktime() const;
		void reset(ClockTime ct, unsigned int num);
		bool pop(ClockTime ct);
		void invalidate();
	private:
		unsigned int m_num;
		ClockTime m_clocktime;
	};

	mp::pthread_mutex m_replace_mutex;
	ReplaceContext m_copying;
	ReplaceContext m_deleting;
@end


@rpc proto_control
	message GetNodesInfo.1 {
	};

	message AttachNewServers.1 {
		bool replace;
	};

	message DetachFaultServers.1 {
		bool replace;
	};

	message CreateBackup.1 {
		std::string suffix;
	};

	message SetAutoReplace.1 {
		bool enable;
	};

	message StartReplace.1 {
	};


public:
	proto_control();
	~proto_control();

public:
	void listen_control(int lsock);

private:
	struct Status : msgpack::define< msgpack::type::tuple<
				msgtype::HSSeed, std::vector<address> > > {
		Status() { }
		msgtype::HSSeed& hsseed() { return get<0>(); }
		std::vector<address>& newcomers() { return get<1>(); }
	};

	RPC_REPLY_DECL(CreateBackup_1, from, res, err, life);
@end


}  // namespace manager
}  // namespace kumo

