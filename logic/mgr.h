#ifndef LOGIC_MGR_H__
#define LOGIC_MGR_H__

#include "logic/cluster.h"
#include "rpc/server.h"
#include "rpc/sbuffer.h"

namespace kumo {


class Manager : public ClusterBase<Manager>, public rpc::cluster {
public:
	template <typename Config>
	Manager(Config& cfg);

	~Manager();

	void cluster_dispatch(
			shared_node& from, role_type role, rpc::weak_responder response,
			method_id method, msgobj param, shared_zone& life);

	void subsystem_dispatch(
			shared_peer& from, rpc::weak_responder response,
			method_id method, msgobj param, shared_zone& life);

	void new_node(address addr, role_type id, shared_node n);
	void lost_node(address addr, role_type id);

	void step_timeout();

public:
	CLUSTER_DECL(KeepAlive);
	CLUSTER_DECL(ReplaceElection);
	CLUSTER_DECL(HashSpaceSync);

	CLUSTER_DECL(ReplaceCopyEnd);
	CLUSTER_DECL(ReplaceDeleteEnd);

	CLUSTER_DECL(WHashSpaceRequest);
	CLUSTER_DECL(RHashSpaceRequest);

	RPC_DECL(HashSpaceRequest);

public:
	void keep_alive();

private:
	// mgr_rpc.cc
	RPC_REPLY_DECL(ResKeepAlive, from, res, err, life);

	void sync_hash_space_servers();
	void sync_hash_space_partner();
	RPC_REPLY_DECL(ResHashSpaceSync, from, res, err, life);

	void push_hash_space_clients();
	RPC_REPLY_DECL(ResHashSpacePush, from, res, err, life);

	// mgr_replace.cc
	void add_server(const address& addr, shared_node& s);
	void remove_server(const address& addr);

	void replace_election();

	RPC_REPLY_DECL(ResReplaceElection, from, res, err, life);

	void start_replace();
	RPC_REPLY_DECL(ResReplaceCopyStart, from, res, err, life);
	RPC_REPLY_DECL(ResReplaceDeleteStart, from, res, err, life);
	void finish_replace_copy();
	void finish_replace();

public:
	// mgr_control.cc
	class ControlConnection;

	static void control_checked_accepted(void* data, int fd);
	void listen_control(int lsock);

	void GetStatus(rpc::responder response);
	void StartReplace(rpc::responder response);
	void CreateBackup(rpc::responder response);

private:
	Clock m_clock;
	HashSpace m_rhs;
	HashSpace m_whs;

	typedef std::vector<weak_session> clients_t;
	clients_t m_clients;

	// 'joined servers' including both active and fault servers
	typedef std::map<address, weak_node> servers_t;
	servers_t m_servers;

	// added but 'not joined servers'
	typedef std::vector<weak_node> newcomer_servers_t;
	newcomer_servers_t m_newcomer_servers;

	address m_partner;

	class ReplaceContext {
	public:
		ReplaceContext();
		~ReplaceContext();
	public:
		ClockTime clocktime() const;
		void reset(ClockTime ct, unsigned int num);
		bool empty() const;
		bool pop(ClockTime ct);
	private:
		unsigned int m_num;
		ClockTime m_clocktime;
	};

	ReplaceContext m_copying;
	ReplaceContext m_deleting;

	bool m_cfg_auto_replace;

	const uint32_t m_cfg_replace_delay_clocks;

	uint64_t m_delayed_replace_clock;
	void delayed_replace_election();
};


template <typename Config>
Manager::Manager(Config& cfg) :
	ClusterBase<Manager>(cfg),
	rpc::cluster(
			protocol::MANAGER,
			cfg.cluster_addr,
			cfg.connect_retry_limit,
			cfg.connect_timeout_steps,
			cfg.reconnect_timeout_msec),
	m_partner(cfg.partner),
	m_cfg_auto_replace(cfg.auto_replace),
	m_cfg_replace_delay_clocks(cfg.replace_delay_clocks),
	m_delayed_replace_clock(0)
{
	LOG_INFO("start manager ",addr());
	listen_cluster(cfg.cluster_lsock);
	listen_control(cfg.ctlsock_lsock);
	start_timeout_step<Manager>(cfg.clock_interval_usec, this);
	start_keepalive<Manager>(cfg.keepalive_interval_usec, this);
}


}  // namespace kumo

#endif /* logic/mgr.h */

