#ifndef RPC_TRANSPORT_H__
#define RPC_TRANSPORT_H__

#include "rpc/session.h"

namespace rpc {


struct transport_manager {
	virtual ~transport_manager() { }
};

class basic_transport {
public:
	basic_transport(basic_shared_session s, transport_manager* mgr = NULL);
	~basic_transport();

public:
	// get transport manager
	transport_manager* get_manager();

public:
	typedef std::auto_ptr<msgpack::zone> auto_zone;

	void process_request(method_id method, msgobj param,
			msgid_t msgid, auto_zone& z);

	void process_response(msgobj res, msgobj err,
			msgid_t msgid, auto_zone& z);

public:
	static void send_data(int fd, const char* buf, size_t buflen, void (*finalize)(void*), void* data);
	static void send_datav(int fd, vrefbuffer* buf, void (*finalize)(void*), void* data);

protected:
	basic_shared_session m_session;

private:
	transport_manager* m_manager;

private:
	basic_transport();
	basic_transport(const basic_transport&);
};


// MULTI-thread context
class transport : public basic_transport, public connection<transport> {
public:
	transport(int fd, basic_shared_session& s, transport_manager* mgr = NULL);

	virtual ~transport();

	typedef std::auto_ptr<msgpack::zone> auto_zone;

	void process_request(method_id method, msgobj param,
			msgid_t msgid, auto_zone& z);

	void process_response(msgobj res, msgobj err,
			msgid_t msgid, auto_zone& z);

private:
	transport();
	transport(const transport&);
};


inline basic_transport::basic_transport(basic_shared_session s, transport_manager* mgr) :
	m_session(s), m_manager(mgr) { }

inline basic_transport::~basic_transport() { }

inline transport_manager* basic_transport::get_manager()
{
	return m_manager;
}

inline void basic_transport::process_request(method_id method, msgobj param,
		msgid_t msgid, auto_zone& z)
{
	if(!m_session) {
		throw std::runtime_error("session unbound");
	}
	m_session->process_request(m_session, method, param, msgid, z);
}

inline void basic_transport::process_response(msgobj result, msgobj error,
		msgid_t msgid, auto_zone& z)
{
	mp::iothreads::submit(&basic_session::process_response, m_session.get(),
			m_session, result, error, msgid, z.get());
	z.release();
}


inline transport::transport(int fd, basic_shared_session& s,
		transport_manager* mgr) :
	basic_transport(s, mgr),
	connection<transport>(fd)
{
	m_session->bind_transport(connection<transport>::fd());
}

inline transport::~transport()
{
	if(m_session) {
		m_session->unbind_transport(connection<transport>::fd(), m_session);
	}
}

inline void transport::process_request(method_id method, msgobj param,
		msgid_t msgid, auto_zone& z)
{
	basic_transport::process_request(method, param, msgid, z);
}

inline void transport::process_response(msgobj res, msgobj err,
		msgid_t msgid, auto_zone& z)
{
	basic_transport::process_response(res, err, msgid, z);
}


}  // namespace rpc

#endif /* rpc/transport.h */

