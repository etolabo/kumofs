#ifndef LOGIC_MGR_IMPL_H__
#define LOGIC_MGR_IMPL_H__

#include "logic/srv.h"

namespace kumo {


#define BIND_RESPONSE(FUNC, ...) \
	mp::bind(&Server::FUNC, this, _1, _2, _3, _4, ##__VA_ARGS__)


#define RPC_FUNC(NAME, from, response, z, param) \
	RPC_IMPL(Server, NAME, from, response, z, param)


#define CLUSTER_FUNC(NAME, from, response, z, param) \
	CLUSTER_IMPL(Server, NAME, from, response, z, param)


#define RPC_REPLY(NAME, from, res, err, life, ...) \
	RPC_REPLY_IMPL(Server, NAME, from, res, err, life, ##__VA_ARGS__)


#if NUM_REPLICATION != 2
#error fix following code
#endif

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

#if 0
namespace {

inline bool clocktime_new(const char* meta, uint64_t clocktime)
{
	return ClockTime(DBFormat(meta, DBFormat::LEADING_METADATA_SIZE).clocktime()) < ClockTime(clocktime);
}

}  // noname namespace
#endif


}  // namespace kumo

#endif /* logic/srv_impl.h */

