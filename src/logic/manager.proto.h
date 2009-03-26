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
	message KeepAlive +cluster {
		Clock adjust_clock;
		// ok: UNDEFINED
	};

	message HashSpaceRequest {
		// success: gateway::proto_network::HashSpacePush
	};

	message WHashSpaceRequest {
		// success: hash_space:tuple<array<raw_ref>,uint64_t>
	};

	message RHashSpaceRequest {
		// success: hash_space:tuple<array<raw_ref>,uint64_t>
	};

	message HashSpaceSync +cluster {
		msgtype::HSSeed wseed;
		msgtype::HSSeed rseed;
		Clock adjust_clock;
		// success: true
		// obsolete: nil
	};

public:
	void keep_alive();

	void sync_hash_space_servers(REQUIRE_HSLK);
	void sync_hash_space_partner(REQUIRE_HSLK);
	void push_hash_space_clients(REQUIRE_HSLK);

private:
	RPC_REPLY_DECL(KeepAlive, from, res, err, z);
	RPC_REPLY_DECL(HashSpaceSync, from, res, err, z);
	RPC_REPLY_DECL(HashSpacePush, from, res, err, z);
@end


@code proto_replace
class framework;
@end

@rpc proto_replace
	message ReplaceCopyEnd +cluster {
		ClockTime replace_time;
		Clock adjust_clock;
		// acknowledge: true
	};

	message ReplaceDeleteEnd +cluster {
		ClockTime replace_time;
		Clock adjust_clock;
		// acknowledge: true
	};

	message ReplaceElection +cluster {
		msgtype::HSSeed hsseed;
		Clock adjust_clock;
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

	RPC_REPLY_DECL(ReplaceElection, from, res, err, z);
	RPC_REPLY_DECL(ReplaceCopyStart, from, res, err, z);
	RPC_REPLY_DECL(ReplaceDeleteStart, from, res, err, z);

	void finish_replace_copy(REQUIRE_RELK);
	void finish_replace(REQUIRE_RELK);

private:
	class progress {
	public:
		progress();
		~progress();
	public:
		typedef std::vector<rpc::address> nodes_t;
		ClockTime clocktime() const;
	public:
		void reset(ClockTime ct, const nodes_t& nodes);
		bool pop(ClockTime ct, const rpc::address& node);
		nodes_t invalidate();
	private:
		nodes_t m_remainder;
		nodes_t m_target_nodes;
		ClockTime m_clocktime;
	};

	mp::pthread_mutex m_replace_mutex;
	progress m_copying;
	progress m_deleting;
@end


@rpc proto_control
	message GetNodesInfo {
	};

	message AttachNewServers {
		bool replace;
	};

	message DetachFaultServers {
		bool replace;
	};

	message CreateBackup {
		std::string suffix;
	};

	message SetAutoReplace {
		bool enable;
	};

	message StartReplace {
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

	RPC_REPLY_DECL(CreateBackup, from, res, err, z);
@end


}  // namespace manager
}  // namespace kumo

