#ifndef LOGIC_SRV_H__
#define LOGIC_SRV_H__

#include "logic/cluster.h"
#include "rpc/server.h"
#include "storage.h"

namespace kumo {


class Server : public ClusterBase<Server>, public rpc::cluster {
public:
	template <typename Config>
	Server(Config& cfg);

	~Server();

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

	CLUSTER_DECL(ReplicateSet);
	CLUSTER_DECL(ReplicateDelete);
	CLUSTER_DECL(HashSpaceSync);
	CLUSTER_DECL(CreateBackup);

	CLUSTER_DECL(ReplaceCopyStart);
	CLUSTER_DECL(ReplaceDeleteStart);
	CLUSTER_DECL(ReplacePropose);
	CLUSTER_DECL(ReplacePush);

	RPC_DECL(Get);
	RPC_DECL(Set);
	RPC_DECL(Delete);

public:
	void keep_alive();

private:
	// srv.cc
	RPC_REPLY_DECL(ResKeepAlive, from, res, err, life);

	void renew_w_hash_space();
	void renew_r_hash_space();
	RPC_REPLY_DECL(ResWHashSpaceRequest, from, res, err, life);
	RPC_REPLY_DECL(ResRHashSpaceRequest, from, res, err, life);

	// srv_store.cc
	void check_replicator_assign(HashSpace& hs, const char* key, uint32_t keylen);
	void check_coordinator_assign(HashSpace& hs, const char* key, uint32_t keylen);

	typedef RPC_RETRY(ReplicateSet) RetryReplicateSet;
	RPC_REPLY_DECL(ResReplicateSet, from, res, err, life,
			RetryReplicateSet* retry,
			unsigned short* copy_required,
			rpc::weak_responder response, uint64_t clocktime);

	typedef RPC_RETRY(ReplicateDelete) RetryReplicateDelete;
	RPC_REPLY_DECL(ResReplicateDelete, from, res, err, life,
			RetryReplicateDelete* retry,
			unsigned short* copy_required,
			rpc::weak_responder response);

	// srv_replace.cc
	static bool test_replicator_assign(HashSpace& hs, uint64_t h, const address& target);

	void replace_delete(shared_node& manager, HashSpace& hs);
	RPC_REPLY_DECL(ResReplaceDeleteEnd, from, res, err, life);

	void propose_replace_push(const address& node,
			const char* key, uint32_t keylen,
			uint64_t metaval_clocktime, shared_zone& life,
			ClockTime replace_time);

	void replace_push(const address& node,
			const char* key, uint32_t keylen,
			const char* meta_val, size_t meta_vallen,
			shared_zone& life, ClockTime replace_time);

	void replace_copy(const address& manager_addr, HashSpace& hs);
	void finish_replace_copy(ClockTime clocktime);
	RPC_REPLY_DECL(ResReplaceCopyEnd, from, res, err, life);

	typedef RPC_RETRY(ReplacePropose) RetryReplacePropose;
	typedef RPC_RETRY(ReplacePush) RetryReplacePush;

	RPC_REPLY_DECL(ResReplacePropose, from, res, err, life,
			RetryReplacePropose* retry, ClockTime replace_time);
	RPC_REPLY_DECL(ResReplacePush, from, res, err, life,
			RetryReplacePush* retry, ClockTime replace_time);

private:
	Clock m_clock;

	mp::pthread_rwlock m_rhs_mutex;
	HashSpace m_rhs;

	mp::pthread_rwlock m_whs_mutex;
	HashSpace m_whs;

	Storage& m_db;

	const address m_manager1;
	const address m_manager2;

	class ReplaceContext {
	public:
		ReplaceContext();
		~ReplaceContext();

	public:
		void reset(const address& mgr, ClockTime ct);

		void proposed(ClockTime ct);
		void propose_returned(ClockTime ct);

		void pushed(ClockTime ct);
		void push_returned(ClockTime ct);

		const address& mgr_addr() const;

		bool is_finished(ClockTime ct) const;
		void invalidate();

	private:
		int m_propose_waiting;
		int m_push_waiting;
		ClockTime m_clocktime;
		address m_mgr;
	};

	mp::pthread_mutex m_replacing_mutex;
	ReplaceContext m_replacing;

	std::string m_cfg_db_backup_basename;

	const unsigned short m_cfg_replicate_set_retry_num;
	const unsigned short m_cfg_replicate_delete_retry_num;
	const unsigned short m_cfg_replace_propose_retry_num;
	const unsigned short m_cfg_replace_push_retry_num;

private:
	Server();
	Server(const Server&);
};


template <typename Config>
Server::Server(Config& cfg) :
	ClusterBase<Server>(cfg),
	rpc::cluster(
			protocol::SERVER,
			cfg.cluster_addr,
			cfg.connect_timeout_msec,
			cfg.connect_retry_limit),
	m_db(*cfg.db),
	m_manager1(cfg.manager1),
	m_manager2(cfg.manager2),
	m_cfg_db_backup_basename(cfg.db_backup_basename),
	m_cfg_replicate_set_retry_num(cfg.replicate_set_retry_num),
	m_cfg_replicate_delete_retry_num(cfg.replicate_delete_retry_num),
	m_cfg_replace_propose_retry_num(cfg.replace_propose_retry_num),
	m_cfg_replace_push_retry_num(cfg.replace_push_retry_num)
{
	LOG_INFO("start server ",addr());
	listen_cluster(cfg.cluster_lsock);
	start_timeout_step(cfg.clock_interval_usec);
	start_keepalive(cfg.keepalive_interval_usec);
}


}  // namespace kumo

#endif /* logic/srv.h */

