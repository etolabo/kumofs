#include "logic/mgr_impl.h"

namespace kumo {


namespace control {
	using msgpack::define;
	using msgpack::type::tuple;
	using msgpack::type::raw_ref;
	typedef HashSpace::Seed HSSeed;

	struct Status : define< tuple<
			HSSeed,
			std::vector<address> > > {
		HSSeed& hsseed() { return get<0>(); }
		std::vector<address>& newcomers() { return get<1>(); }
	};

	enum command_type {
		GetStatus			= 84,
		AttachNewServers   	= 85,
		DetachFaultServers	= 86,
		CreateBackup		= 87,
		SetAutoReplace      = 88,
	};
}  // namespace control


void Manager::GetStatus(rpc::responder response)
{
	// FIXME stub
	control::Status res;
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

void Manager::AttachNewServers(rpc::responder response)
{
	attach_new_servers();
	start_replace();
	response.null();
}

void Manager::DetachFaultServers(rpc::responder response)
{
	detach_fault_servers();
	start_replace();
	response.null();
}

//void Manager::SetAutoReplace(rpc::response response)
//{
//}

void Manager::CreateBackup(rpc::responder response)
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

private:
	static void GetStatus(rpc::responder response, Manager* mgr)
	{
		mgr->GetStatus(response);
	}

	static void AttachNewServers(rpc::responder response, Manager* mgr)
	{
		mgr->AttachNewServers(response);
	}

	static void DetachFaultServers(rpc::responder response, Manager* mgr)
	{
		mgr->DetachFaultServers(response);
	}

	static void CreateBackup(rpc::responder response, Manager* mgr)
	{
		mgr->CreateBackup(response);
	}

public:
	void dispatch_request(method_id method, msgobj param, rpc::responder& response, auto_zone& z);
	void process_response(msgobj result, msgobj error, msgid_t msgid, auto_zone& z);

private:
	Manager* m_mgr;

private:
	ControlConnection();
	ControlConnection(const ControlConnection&);
};


void Manager::ControlConnection::dispatch_request(method_id method, msgobj param, rpc::responder& response, auto_zone& z)
{
	LOG_TRACE("receive control message");
	switch((control::command_type)method) {
	case control::GetStatus:
		iothreads::submit(&ControlConnection::GetStatus, response, m_mgr);
		break;
	case control::AttachNewServers:
		iothreads::submit(&ControlConnection::AttachNewServers, response, m_mgr);
		break;
	case control::DetachFaultServers:
		iothreads::submit(&ControlConnection::DetachFaultServers, response, m_mgr);
		break;
	case control::CreateBackup:
		iothreads::submit(&ControlConnection::CreateBackup, response, m_mgr);
		break;
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

