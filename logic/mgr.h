#ifndef LOGIC_MGR_H__
#define LOGIC_MGR_H__

#include "logic/cluster.h"
#include "logic/mgr_control.h"
#include "rpc/server.h"
#include "rpc/sbuffer.h"

namespace kumo {


class Manager : public ClusterBase<Manager>, public rpc::cluster {
public:
	template <typename Config>
	Manager(Config& cfg);

	~Manager();

	void cluster_dispatch(
			shared_node from, weak_responder response,
			method_id method, msgobj param, auto_zone z);

	void subsystem_dispatch(
			shared_peer from, weak_responder response,
			method_id method, msgobj param, auto_zone z);

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

#define REQUIRE_SSLK const pthread_scoped_lock& sslk
#define REQUIRE_HSLK const pthread_scoped_lock& hslk
	void sync_hash_space_servers(REQUIRE_HSLK, REQUIRE_SSLK);
	void sync_hash_space_partner(REQUIRE_HSLK);
	void push_hash_space_clients(REQUIRE_HSLK);

	RPC_REPLY_DECL(ResHashSpaceSync, from, res, err, life);
	RPC_REPLY_DECL(ResHashSpacePush, from, res, err, life);

	// mgr_replace.cc
	void add_server(const address& addr, shared_node& s);
	void remove_server(const address& addr);

	void replace_election();

	RPC_REPLY_DECL(ResReplaceElection, from, res, err, life);

	void attach_new_servers(REQUIRE_HSLK);
	void detach_fault_servers(REQUIRE_HSLK);

	void start_replace(REQUIRE_HSLK);
	RPC_REPLY_DECL(ResReplaceCopyStart, from, res, err, life);
	RPC_REPLY_DECL(ResReplaceDeleteStart, from, res, err, life);

#define REQUIRE_RELK const pthread_scoped_lock& relk
	void finish_replace_copy(REQUIRE_RELK);
	void finish_replace(REQUIRE_RELK);

public:
	// mgr_control.cc
	class ControlConnection;

	void control_checked_accepted(int fd, int err);
	void listen_control(int lsock);

	CONTROL_DECL(GetStatus);
	CONTROL_DECL(AttachNewServers);
	CONTROL_DECL(DetachFaultServers);
	CONTROL_DECL(CreateBackup);
	CONTROL_DECL(SetAutoReplace);
	CONTROL_DECL(StartReplace);

	RPC_REPLY_DECL(ResCreateBackup, from, res, err, life);

private:
	Clock m_clock;

	mp::pthread_mutex m_hs_mutex;
	HashSpace m_rhs;
	HashSpace m_whs;

	// 'joined servers' including both active and fault servers
	mp::pthread_mutex m_servers_mutex;
	typedef std::map<address, weak_node> servers_t;
	servers_t m_servers;

	// added but 'not joined servers'
	mp::pthread_mutex m_new_servers_mutex;
	typedef std::vector<weak_node> new_servers_t;
	new_servers_t m_new_servers;

	const address m_partner;

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

	bool m_cfg_auto_replace;

	const short m_cfg_replace_delay_clocks;

	short m_delayed_replace_clock;
	void delayed_replace_election();
};


template <typename Config>
Manager::Manager(Config& cfg) :
	ClusterBase<Manager>(cfg),
	rpc::cluster(
			protocol::MANAGER,
			cfg.cluster_addr,
			cfg.connect_timeout_msec,
			cfg.connect_retry_limit),
	m_partner(cfg.partner),
	m_cfg_auto_replace(cfg.auto_replace),
	m_cfg_replace_delay_clocks(cfg.replace_delay_clocks),
	m_delayed_replace_clock(0)
{
	LOG_INFO("start manager ",addr());
	listen_cluster(cfg.cluster_lsock);
	listen_control(cfg.ctlsock_lsock);
	start_timeout_step(cfg.clock_interval_usec);
	start_keepalive(cfg.keepalive_interval_usec);
}


}  // namespace kumo

#endif /* logic/mgr.h */

