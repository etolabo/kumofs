#ifndef SERVER_FRAMEWORK_H__
#define SERVER_FRAMEWORK_H__

#include "logic/cluster_logic.h"
#include "logic/clock_logic.h"
#include "server/mod_control.h"
#include "server/mod_network.h"
#include "server/mod_replace.h"
#include "server/mod_replace_stream.h"
#include "server/mod_store.h"

#define EACH_ASSIGN(HS, HASH, REAL, CODE) \
{ \
	HashSpace::iterator _it_(HS.find(HASH)); \
	HashSpace::iterator _origin_(_it_); \
	HashSpace::node REAL; \
	REAL = *_it_; \
	CODE; \
	++_it_; \
	for(; _it_ != _origin_; ++_it_) { \
		if(*_it_ == *_origin_) { continue; } \
		HashSpace::node _rep1_ = *_it_; \
		REAL = _rep1_; \
		CODE; \
		++_it_; \
		for(; _it_ != _origin_; ++_it_) { \
			if(*_it_ == *_origin_ || *_it_ == _rep1_) { continue; } \
			HashSpace::node _rep2_ = *_it_; \
			REAL = _rep2_; \
			CODE; \
			break; \
		} \
		break; \
	} \
}

namespace kumo {
namespace server {


class framework : public cluster_logic<framework>, public clock_logic {
public:
	template <typename Config>
	framework(const Config& cfg);

	template <typename Config>
	void run(const Config& cfg);

	void cluster_dispatch(
			shared_node from, weak_responder response,
			rpc::method_id method, rpc::msgobj param, auto_zone z);

	void subsystem_dispatch(
			shared_peer from, weak_responder response,
			rpc::method_id method, rpc::msgobj param, auto_zone z);

	void new_node(address addr, role_type id, shared_node n);
	void lost_node(address addr, role_type id);

	// override wavy_server::end_preprocess
	virtual void end_preprocess();

	// rpc_server
	void keep_alive()
	{
		mod_network.keep_alive();
	}

	// override rpc_server<framework>::timer_handler
	void timer_handler()
	{
		clock_logic::clock_update_time();
		rpc_server<framework>::timer_handler();
	}

public:
	mod_network_t mod_network;
	mod_control_t mod_control;
	mod_replace_t mod_replace;
	mod_store_t   mod_store;
	mod_replace_stream_t mod_replace_stream;

	// FIXME mod_replace_stream::rpc_ReplaceOffer_1
	unsigned int connect_timeout_msec() const {
		return m_connect_timeout_msec;  // rpc::client<>
	}

private:
	framework();
	framework(const framework&);
};


class resource {
public:
	template <typename Config>
	resource(const Config& cfg);

private:
	mp::pthread_rwlock m_rhs_mutex;
	HashSpace m_rhs;

	mp::pthread_rwlock m_whs_mutex;
	HashSpace m_whs;

	Storage& m_db;

	const address m_manager1;
	const address m_manager2;

private:
	std::string m_cfg_offer_tmpdir;
	std::string m_cfg_db_backup_basename;

	const unsigned short m_cfg_replicate_set_retry_num;
	const unsigned short m_cfg_replicate_delete_retry_num;

	const time_t m_stat_start_time;  // FIXME m_start_time -> m_stat_start_time
	volatile uint64_t m_stat_num_get;
	volatile uint64_t m_stat_num_set;
	volatile uint64_t m_stat_num_delete;

public:
	RESOURCE_ACCESSOR(mp::pthread_rwlock, rhs_mutex);
	RESOURCE_ACCESSOR(mp::pthread_rwlock, whs_mutex);
	RESOURCE_ACCESSOR(HashSpace, rhs);
	RESOURCE_ACCESSOR(HashSpace, whs);

	RESOURCE_ACCESSOR(Storage, db);

	RESOURCE_CONST_ACCESSOR(address, manager1);
	RESOURCE_CONST_ACCESSOR(address, manager2);

	RESOURCE_CONST_ACCESSOR(std::string, cfg_offer_tmpdir);
	RESOURCE_CONST_ACCESSOR(std::string, cfg_db_backup_basename);
	RESOURCE_CONST_ACCESSOR(unsigned short, cfg_replicate_set_retry_num);
	RESOURCE_CONST_ACCESSOR(unsigned short, cfg_replicate_delete_retry_num);

	RESOURCE_CONST_ACCESSOR(time_t, stat_start_time);

	// FIXME incr_stat_num_{get,set,delete} + const accessor
	RESOURCE_ACCESSOR(volatile uint64_t, stat_num_get);
	RESOURCE_ACCESSOR(volatile uint64_t, stat_num_set);
	RESOURCE_ACCESSOR(volatile uint64_t, stat_num_delete);

private:
	resource();
	resource(const resource&);
};


extern std::auto_ptr<framework> net;
extern std::auto_ptr<resource> share;


}  // namespace server
}  // namespace kumo

#endif  /* server/framework.h */

