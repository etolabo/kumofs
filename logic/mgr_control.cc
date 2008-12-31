#include "logic/mgr_impl.h"
#include "logic/mgr_control.h"

namespace kumo {


CONTROL_IMPL(GetStatus, param, response)
try {
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
CONTROL_CATCH(GetStatus, response);

CONTROL_IMPL(AttachNewServers, param, response)
try {
	attach_new_servers();
	start_replace();
	response.null();
}
CONTROL_CATCH(AttachNewServers, response);

CONTROL_IMPL(DetachFaultServers, param, response)
try {
	detach_fault_servers();
	start_replace();
	response.null();
}
CONTROL_CATCH(DetachFaultServers, response);

CONTROL_IMPL(CreateBackup, param, response)
try {
	if(param.suffix().empty()) {
		std::string msg("empty suffix");
		response.error(msg);
		return;
	}
	protocol::type::CreateBackup arg(param.suffix());
	rpc::callback_t callback( BIND_RESPONSE(ResCreateBackup) );
	shared_zone nullz;
	EACH_ACTIVE_SERVERS_BEGIN(node)
		node->call(protocol::CreateBackup, arg, nullz, callback, 10);
	EACH_ACTIVE_SERVERS_END
	response.null();
}
CONTROL_CATCH(CreateBackup, response);

RPC_REPLY(ResCreateBackup, from, res, err, life)
{ }

CONTROL_IMPL(SetAutoReplace, param, response)
try {
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
CONTROL_CATCH(SetAutoReplace, response);

CONTROL_IMPL(StartReplace, param, response)
try {
	start_replace();
	response.null();
}
CONTROL_CATCH(StartReplace, response);


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
		m_mgr->NAME(param.as<control::type::NAME>(), response); \
		break;

//		iothreads::submit(&Manager::NAME, m_mgr, param.as<control::type::NAME>(), response);
//		break

void Manager::ControlConnection::dispatch_request(method_id method, msgobj param, rpc::responder& response, auto_zone& z)
{
	LOG_TRACE("receive control message: ",param);
	switch((control::command_type)method) {
	CONTROL_DISPATCH(GetStatus);
	CONTROL_DISPATCH(AttachNewServers);
	CONTROL_DISPATCH(DetachFaultServers);
	CONTROL_DISPATCH(CreateBackup);
	CONTROL_DISPATCH(SetAutoReplace);
	CONTROL_DISPATCH(StartReplace);
	default:
		throw std::runtime_error("unknown method");
	}
}

void Manager::control_checked_accepted(int fd, int err)
{
	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(err));
		signal_end(SIGTERM);
		return;
	}
	wavy::add<ControlConnection>(fd, this);
}

void Manager::listen_control(int lsock)
{
	using namespace mp::placeholders;
	wavy::listen(lsock, mp::bind(
				&Manager::control_checked_accepted, this,
				_1, _2));
}

void Manager::ControlConnection::process_response(msgobj result, msgobj error, msgid_t msgid, auto_zone& z)
{
	throw msgpack::type_error();
}


}  // namespace kumo

