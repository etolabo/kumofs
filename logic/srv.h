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

	// override wavy_server::run
	virtual void run();
	virtual void end_preprocess();

public:
	CLUSTER_DECL(KeepAlive);

	CLUSTER_DECL(ReplicateSet);
	CLUSTER_DECL(ReplicateDelete);
	CLUSTER_DECL(HashSpaceSync);
	CLUSTER_DECL(CreateBackup);

	CLUSTER_DECL(ReplaceCopyStart);
	CLUSTER_DECL(ReplaceDeleteStart);
	CLUSTER_DECL(ReplaceOffer);

	RPC_DECL(Get);
	RPC_DECL(Set);
	RPC_DECL(Delete);

	RPC_DECL(GetStatus);

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
	void check_replicator_assign(HashSpace& hs, uint64_t h);
	void check_coordinator_assign(HashSpace& hs, uint64_t h);

	bool SetByRhsWhs(weak_responder response, auto_zone& z,
			protocol::type::DBKey& key, protocol::type::DBValue& val,
			bool is_async);
	void SetByWhs(weak_responder response, auto_zone& z,
			protocol::type::DBKey& key, protocol::type::DBValue& val,
			bool is_async);

	typedef RPC_RETRY(ReplicateSet) RetryReplicateSet;
	RPC_REPLY_DECL(ResReplicateSet, from, res, err, life,
			RetryReplicateSet* retry,
			volatile unsigned int* copy_required,
			rpc::weak_responder response, uint64_t clocktime);

	bool DeleteByRhsWhs(weak_responder response, auto_zone& z,
			protocol::type::DBKey& key,
			bool is_async);
	void DeleteByWhs(weak_responder response, auto_zone& z,
			protocol::type::DBKey& key,
			bool is_async);

	typedef RPC_RETRY(ReplicateDelete) RetryReplicateDelete;
	RPC_REPLY_DECL(ResReplicateDelete, from, res, err, life,
			RetryReplicateDelete* retry,
			volatile unsigned int* copy_required,
			rpc::weak_responder response, bool deleted);

	// srv_replace.cc
	static bool test_replicator_assign(HashSpace& hs, uint64_t h, const address& target);

	void replace_delete(shared_node& manager, HashSpace& hs);
	RPC_REPLY_DECL(ResReplaceDeleteEnd, from, res, err, life);

#define REQUIRE_RELK const pthread_scoped_lock& relk
	void replace_copy(const address& manager_addr, HashSpace& hs);
	void finish_replace_copy(ClockTime clocktime, REQUIRE_RELK);
	RPC_REPLY_DECL(ResReplaceCopyEnd, from, res, err, life);

private:
	Clock m_clock;

	mp::pthread_rwlock m_rhs_mutex;
	HashSpace m_rhs;

	mp::pthread_rwlock m_whs_mutex;
	HashSpace m_whs;

	Storage& m_db;

	const address m_manager1;
	const address m_manager2;

	int m_stream_lsock;
	address m_stream_addr;

private:
	// srv_offer.cc
	void init_stream(int lsock);
	void run_stream();
	void stop_stream();

	class OfferStorage {
	public:
		OfferStorage(const std::string& basename,
				const address& addr, ClockTime replace_time);
		~OfferStorage();
	public:
		void add(const char* key, size_t keylen,
				const char* val, size_t vallen);
		void send(int sock);

		const address& addr() const { return m_addr; }
		ClockTime replace_time() const { return m_replace_time; }
	private:
		address m_addr;
		ClockTime m_replace_time;

		struct scoped_fd {
			scoped_fd(int fd) : m(fd) { }
			~scoped_fd() { ::close(m); }
			int get() { return m; }
		private:
			int m;
			scoped_fd();
			scoped_fd(const scoped_fd&);
		};
		static int openfd(const std::string& basename);
		scoped_fd m_fd;

		class mmap_stream;
		std::auto_ptr<mmap_stream> m_mmap;
	private:
		OfferStorage();
		OfferStorage(const OfferStorage&);
	};

	typedef mp::shared_ptr<OfferStorage> SharedOfferStorage;
	typedef std::vector<SharedOfferStorage> SharedOfferMap;
	struct SharedOfferMapComp;

	mp::pthread_mutex m_offer_map_mutex;
	SharedOfferMap m_offer_map;
	static SharedOfferMap::iterator find_offer_map(
			SharedOfferMap& map, const address& addr);

	class OfferStorageMap {
	public:
		OfferStorageMap(const std::string& basename, ClockTime replace_time);
		~OfferStorageMap();
	public:
		void add(const address& addr,
				const char* key, size_t keylen,
				const char* val, size_t vallen);
		void commit(SharedOfferMap* dst);
	private:
		SharedOfferMap m_map;
		const std::string& m_basename;
		ClockTime m_replace_time;
	private:
		OfferStorageMap();
		OfferStorageMap(const OfferStorageMap&);
	};

	void send_offer(OfferStorageMap& offer, ClockTime replace_time);
	RPC_REPLY_DECL(ResReplaceOffer, from, res, err, life,
			ClockTime replace_time, address addr);

	void stream_accepted(int fd, int err);
	void stream_connected(int fd, int err);

	std::auto_ptr<mp::wavy::core> m_stream_core;
	class OfferStreamHandler;
	friend class OfferStreamHandler;

private:
	mp::pthread_mutex m_replacing_mutex;

	class ReplaceContext {
	public:
		ReplaceContext();
		~ReplaceContext();
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
	ReplaceContext m_replacing;
	void replace_offer_start(ClockTime replace_time, REQUIRE_RELK);
	void replace_offer_finished(ClockTime replace_time, REQUIRE_RELK);

	std::string m_cfg_offer_tmpdir;
	std::string m_cfg_db_backup_basename;

	const unsigned short m_cfg_replicate_set_retry_num;
	const unsigned short m_cfg_replicate_delete_retry_num;

	const time_t m_start_time;
	volatile uint64_t m_stat_num_get;
	volatile uint64_t m_stat_num_set;
	volatile uint64_t m_stat_num_delete;

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
	m_stream_addr(cfg.stream_addr),

	m_cfg_offer_tmpdir(cfg.offer_tmpdir),
	m_cfg_db_backup_basename(cfg.db_backup_basename),
	m_cfg_replicate_set_retry_num(cfg.replicate_set_retry_num),
	m_cfg_replicate_delete_retry_num(cfg.replicate_delete_retry_num),

	m_start_time(time(NULL)),
	m_stat_num_get(0),
	m_stat_num_set(0),
	m_stat_num_delete(0)
{
	LOG_INFO("start server ",addr());
	MLOGPACK("SS",1, "Server start",
			"time", time(NULL),
			"addr", cfg.cluster_addr,
			"db",cfg.dbpath,
			"mgr1", m_manager1,
			"mgr2", m_manager2,
			"sadd", m_stream_addr,
			"tmpd", m_cfg_offer_tmpdir,
			"bkup", m_cfg_db_backup_basename);
	listen_cluster(cfg.cluster_lsock);
	init_stream(cfg.stream_lsock);
	start_timeout_step(cfg.clock_interval_usec);
	start_keepalive(cfg.keepalive_interval_usec);
}

inline void Server::run()
{
	wavy_server::run();
	run_stream();
	// FIXME end
}

inline void Server::end_preprocess()
{
	stop_stream();
}


}  // namespace kumo

#endif /* logic/srv.h */

