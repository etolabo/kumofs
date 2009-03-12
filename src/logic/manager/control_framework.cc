#include "manager/framework.h"
#include "manager/control_framework.h"

namespace kumo {
namespace manager {


control_framework::control_framework() { }
control_framework::~control_framework() { }

void control_framework::dispatch(
		rpc::shared_peer from, rpc::weak_responder response,
		rpc::method_id method, rpc::msgobj param, rpc::auto_zone z)
{
	// FIXME try & catch
	switch(method.get()) {
	RPC_DISPATCH(proto_control, GetStatus_1);
	RPC_DISPATCH(proto_control, AttachNewServers_1);
	RPC_DISPATCH(proto_control, DetachFaultServers_1);
	RPC_DISPATCH(proto_control, CreateBackup_1);
	RPC_DISPATCH(proto_control, SetAutoReplace_1);
	RPC_DISPATCH(proto_control, StartReplace_1);
	default:
		// FIXME exception class
		throw std::runtime_error("unknown method");
	}
}


void control_framework::listen_control(int lsock)
{
	using namespace mp::placeholders;
	wavy::listen(lsock, mp::bind(
				&control_framework::control_checked_accepted, this,
				_1, _2));
}

void control_framework::control_checked_accepted(int fd, int err)
{
	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(err));
		net->signal_end();
		return;
	}
	accepted(fd);
}


}  // namespace manager
}  // namespace kumo

