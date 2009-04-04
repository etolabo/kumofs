#ifndef GATEWAY_FRAMEWORK_H__
#define GATEWAY_FRAMEWORK_H__

#include "logic/client_logic.h"
#include "gateway/mod_network.h"
#include "gateway/mod_store.h"

namespace kumo {
namespace gateway {


class framework : public client_logic<framework> {
public:
	template <typename Config>
	framework(const Config& cfg);

	template <typename Config>
	void run(const Config& cfg);

	void dispatch(
			shared_session from, weak_responder response,
			rpc::method_id method, rpc::msgobj param, auto_zone z);

	void session_lost(const address& addr, shared_session& s);

	// rpc_server
	void keep_alive()
	{
		mod_network.keep_alive();
	}

public:
	mod_network_t mod_network;
	mod_store_t   mod_store;

private:
	framework();
	framework(const framework&);
};


class resource {
public:
	template <typename Config>
	resource(const Config& cfg);

private:
	mp::pthread_rwlock m_hs_rwlock;
	HashSpace m_rhs;
	HashSpace m_whs;

	const address m_manager1;
	const address m_manager2;

	const bool m_cfg_async_replicate_set;
	const bool m_cfg_async_replicate_delete;

	const unsigned short m_cfg_get_retry_num;
	const unsigned short m_cfg_set_retry_num;
	const unsigned short m_cfg_delete_retry_num;

	const unsigned short m_cfg_renew_threshold;

	unsigned short m_error_count;

public:
	// mod_store.cc
	void incr_error_renew_count();

public:
	RESOURCE_ACCESSOR(mp::pthread_rwlock, hs_rwlock);
	bool update_rhs(const HashSpace::Seed& seed, REQUIRE_HSLK_WRLOCK);
	bool update_whs(const HashSpace::Seed& seed, REQUIRE_HSLK_WRLOCK);

	enum hash_space_type {
		HS_WRITE,
		HS_READ,
	};

	// mod_store.cc
	// Note: hslk is not required
	template <hash_space_type Hs>
	shared_session server_for(uint64_t h, unsigned int offset = 0);

public:
	RESOURCE_CONST_ACCESSOR(address, manager1);
	RESOURCE_CONST_ACCESSOR(address, manager2);

	RESOURCE_CONST_ACCESSOR(bool, cfg_async_replicate_set);
	RESOURCE_CONST_ACCESSOR(bool, cfg_async_replicate_delete);

	RESOURCE_CONST_ACCESSOR(unsigned short, cfg_get_retry_num);
	RESOURCE_CONST_ACCESSOR(unsigned short, cfg_set_retry_num);
	RESOURCE_CONST_ACCESSOR(unsigned short, cfg_delete_retry_num);

	RESOURCE_CONST_ACCESSOR(unsigned short, cfg_renew_threshold);

private:
	resource();
	resource(const resource&);
};


extern std::auto_ptr<framework> net;
extern std::auto_ptr<resource> share;


inline bool resource::update_rhs(const HashSpace::Seed& seed, REQUIRE_HSLK_WRLOCK)
{
	if(m_rhs.empty() ||
			(m_rhs.clocktime() <= seed.clocktime() && !seed.empty())) {
		m_rhs = HashSpace(seed);
		return true;
	} else {
		return false;
	}
}

inline bool resource::update_whs(const HashSpace::Seed& seed, REQUIRE_HSLK_WRLOCK)
{
	if(m_whs.empty() ||
			(m_whs.clocktime() <= seed.clocktime() && !seed.empty())) {
		m_whs = HashSpace(seed);
		return true;
	} else {
		return false;
	}
}


}  // namespace gateway
}  // namespace kumo

#endif  /* gateway/framework.h */

