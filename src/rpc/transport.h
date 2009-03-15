#ifndef RPC_TRANSPORT_H__
#define RPC_TRANSPORT_H__

#include "rpc/types.h"
#include "rpc/connection.h"

namespace rpc {


struct transport_manager {
	virtual ~transport_manager() { }
};


class basic_transport {
public:
	basic_transport(int fd, basic_shared_session s,
			transport_manager* mgr = NULL);
	~basic_transport();

public:
	// get transport manager
	transport_manager* get_manager();

	// called from basic_session::shutdown()
	basic_shared_session shutdown();

public:
	void process_request(method_id method, msgobj param,
			msgid_t msgid, auto_zone& z);

	void process_response(msgobj res, msgobj err,
			msgid_t msgid, auto_zone& z);

public:
	void send_data(const char* buf, size_t buflen,
			void (*finalize)(void*), void* data);

	void send_datav(vrefbuffer* buf,
			void (*finalize)(void*), void* data);

protected:
	int m_fd;
	basic_shared_session m_session;

private:
	transport_manager* m_manager;

private:
	basic_transport();
	basic_transport(const basic_transport&);
};


class transport : public basic_transport, public connection<transport> {
public:
	transport(int fd, basic_shared_session& s,
			transport_manager* mgr = NULL);

	virtual ~transport();

	void process_request(method_id method, msgobj param,
			msgid_t msgid, auto_zone& z);

	void process_response(msgobj res, msgobj err,
			msgid_t msgid, auto_zone& z);

private:
	transport();
	transport(const transport&);
};


}  // namespace rpc

#endif /* rpc/transport.h */

