#ifndef RPC_TRANSPORT_IMPL_H__
#define RPC_TRANSPORT_IMPL_H__

namespace rpc {


inline basic_transport::basic_transport(int fd,
		basic_shared_session s, transport_manager* mgr) :
	m_fd(fd),
	m_session(s),
	m_manager(mgr) { }

inline basic_transport::~basic_transport() { }


inline transport_manager* basic_transport::get_manager()
{
	return m_manager;
}


inline transport::transport(int fd, basic_shared_session& s,
		transport_manager* mgr) :
	basic_transport(fd, s, mgr),
	connection<transport>(fd)
{
	m_session->bind_transport(this);
}

inline transport::~transport()
{
	if(m_session) {
		m_session->unbind_transport(this, m_session);
	}
}


inline void basic_transport::process_request(method_id method, msgobj param,
		msgid_t msgid, auto_zone& z)
{
	if(!m_session) {
		throw std::runtime_error("session unbound");
	}
	m_session->process_request(m_session, method, param, msgid, z);
}

inline void transport::process_request(method_id method, msgobj param,
		msgid_t msgid, auto_zone& z)
{
	basic_transport::process_request(method, param, msgid, z);
}


inline void basic_transport::process_response(msgobj result, msgobj error,
		msgid_t msgid, auto_zone& z)
{
	m_session->process_response(m_session, result, error, msgid, z);
}

inline void transport::process_response(msgobj res, msgobj err,
		msgid_t msgid, auto_zone& z)
{
	basic_transport::process_response(res, err, msgid, z);
}


inline void basic_transport::send_data(
		const char* buf, size_t buflen,
		void (*finalize)(void*), void* data)
{
	wavy::request req(finalize, data);
	wavy::write(m_fd, buf, buflen, req);
}

inline void basic_transport::send_datav(
		vrefbuffer* buf,
		void (*finalize)(void*), void* data)
{
	size_t sz = buf->vector_size();
	struct iovec* vb = (struct iovec*)::malloc(
			sz * sizeof(struct iovec));
	if(!vb) { throw std::bad_alloc(); }

	try {
		buf->get_vector(vb);
		wavy::request req(finalize, data);
		wavy::writev(m_fd, vb, sz, req);

	} catch (...) {
		free(vb);
		throw;
	}
	free(vb);
}


}  // namespace rpc

#endif /* rpc/transport_impl.h */

