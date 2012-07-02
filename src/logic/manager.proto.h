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
#include "manager/proto.h"
#include "logic/msgtype.h"
#include "logic/cluster_logic.h"
#include <msgpack.hpp>
#include <string>
#include <stdint.h>

namespace kumo {
namespace manager {


@message mod_network_t::KeepAlive           =   0
@message mod_network_t::HashSpaceRequest    =   1
@message mod_network_t::HashSpaceSync       =   2
@message mod_network_t::WHashSpaceRequest   =   4
@message mod_network_t::RHashSpaceRequest   =   5
@message mod_replace_t::ReplaceCopyEnd      =  10
@message mod_replace_t::ReplaceDeleteEnd    =  11
@message mod_replace_t::ReplaceElection     =  12
@message mod_control_t::GetNodesInfo        =  99
@message mod_control_t::AttachNewServers    = 100
@message mod_control_t::DetachFaultServers  = 101
@message mod_control_t::CreateBackup        = 102
@message mod_control_t::SetAutoReplace      = 103
@message mod_control_t::StartReplace        = 104
@message mod_control_t::RemoveServer        = 200


@rpc mod_network_t
	message KeepAlive +cluster {
		Clock adjust_clock;
		// ok: UNDEFINED
	};

	message HashSpaceRequest {
		// success: gateway::mod_network_t::HashSpacePush
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


@code mod_replace_t
class framework;
@end

@rpc mod_replace_t
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
	mod_replace_t();
	~mod_replace_t();

public:
	void attach_new_servers(REQUIRE_HSLK);
	void detach_fault_servers(REQUIRE_HSLK);

	void start_replace(REQUIRE_HSLK, bool full = false);
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


@rpc mod_control_t
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
		bool full = false;
	};

	message RemoveServer {
		std::string serverAddr;
	};

public:
	mod_control_t();
	~mod_control_t();

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

