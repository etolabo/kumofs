#include "logic/mgr_impl.h"
#include "logic/mgr_control.h"

namespace kumo {


CONTROL_IMPL(GetStatus, param, response)
{
	control::type::Status res;
	res.hsseed() = HashSpace::Seed(m_whs);
	for(newcomer_servers_t::iterator it(m_newcomer_servers.begin()), it_end(m_newcomer_servers.end());
			it != it_end; ++it) {
		shared_node n(it->lock());
		if(n) {
			res.newcomers().push_back(n->addr());
		}
	}
	response.result(res);
}

CONTROL_IMPL(AttachNewServers, param, response)
{
	attach_new_servers();
	start_replace();
	response.null();
}

CONTROL_IMPL(DetachFaultServers, param, response)
{
	detach_fault_servers();
	start_replace();
	response.null();
}

CONTROL_IMPL(SetAutoReplace, param, response)
{
	// FIXME stub
	if(m_cfg_auto_replace && !param.enable()) {
		m_cfg_auto_replace = false;
		response.result(false);
	} else if(!m_cfg_auto_replace && param.enable()) {
		m_cfg_auto_replace = true;
		attach_new_servers();
		detach_fault_servers();
		response.result(true);
	}
	response.null();
}

CONTROL_IMPL(CreateBackup, param, response)
{
	// FIXME stub
	response.null();
}


class Manager::ControlConnection : public rpc::connection<ControlConnection> {
public:
	ControlConnection(int fd, Manager* mgr) :
		rpc::connection<ControlConnection>(fd),
		m_mgr(mgr) { }

	~ControlConnection() { }

public:
	void dispatch_request(method_id method, msgobj param, rpc::responder& response, auto_zone& z);
	void process_response(msgobj result, msgobj error, msgid_t msgid, auto_zone& z);

private:
	Manager* m_mgr;

private:
	ControlConnection();
	ControlConnection(const ControlConnection&);
};


#define CONTROL_DISPATCH(NAME) \
	case control::NAME: \
		iothreads::submit(&Manager::NAME, m_mgr, param.as<control::type::NAME>(), response); \
		break

void Manager::ControlConnection::dispatch_request(method_id method, msgobj param, rpc::responder& response, auto_zone& z)
{
	LOG_TRACE("receive control message");
	switch((control::command_type)method) {
	CONTROL_DISPATCH(GetStatus);
	CONTROL_DISPATCH(AttachNewServers);
	CONTROL_DISPATCH(DetachFaultServers);
	CONTROL_DISPATCH(CreateBackup);
	default:
		throw std::runtime_error("unknown method");
	}
}

void Manager::control_checked_accepted(void* data, int fd)
{
	Manager* self = reinterpret_cast<Manager*>(data);
	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(-fd));
		self->signal_end(SIGTERM);
		return;
	}
	mp::set_nonblock(fd);
	mp::iothreads::add<ControlConnection>(fd, self);
}

void Manager::listen_control(int lsock)
{
	mp::iothreads::listen(lsock,
			&Manager::control_checked_accepted,
			reinterpret_cast<void*>(this));
}

void Manager::ControlConnection::process_response(msgobj result, msgobj error, msgid_t msgid, auto_zone& z)
{
	throw msgpack::type_error();
}


}  // namespace kumo

