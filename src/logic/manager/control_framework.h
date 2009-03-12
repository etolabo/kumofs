#ifndef MANAGER_CONTROL_FRAMEWORK_H__
#define MANAGER_CONTROL_FRAMEWORK_H__

#include "logic/rpc_server.h"
#include "manager/proto_control.h"

namespace kumo {
namespace manager {


class control_framework : public rpc::server {
public:
	control_framework();
	~control_framework();

	void dispatch(
			rpc::shared_peer from, rpc::weak_responder response,
			rpc::method_id method, rpc::msgobj param, rpc::auto_zone z);

	void listen_control(int lsock);

private:
	proto_control m_proto_control;

private:
	void control_checked_accepted(int fd, int err);

private:
	control_framework(const control_framework&);
};


}  // namespace manager
}  // namespace kumo

#endif  /* manager/framework.h */

