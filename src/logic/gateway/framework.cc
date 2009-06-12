#include "gateway/framework.h"

namespace kumo {
namespace gateway {


std::auto_ptr<framework> net;
std::auto_ptr<resource> share;


void framework::dispatch(
		shared_session from, weak_responder response,
		rpc::method_id method, rpc::msgobj param, auto_zone z)
try {
	switch(method.get()) {
	RPC_DISPATCH(mod_network, HashSpacePush);
	default:
		throw unknown_method_error();
	}
}
DISPATCH_CATCH(method, response)


void framework::session_lost(const address& addr, shared_session& s)
{
	LOG_WARN("lost session ",addr);
	if(addr == share->manager1() || addr == share->manager2()) {
		mod_network.renew_hash_space_for(addr);
	}
}


}  // namespace gateway
}  // namespace kumo

