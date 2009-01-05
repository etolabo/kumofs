#ifndef RPC_RESPONDER_IMPL_H__
#define RPC_RESPONDER_IMPL_H__

#include <mp/object_callback.h>

namespace rpc {


inline responder::responder(int fd, msgid_t msgid) :
	m_fd(fd), m_msgid(msgid) { }

inline responder::~responder() { }

template <typename Result>
void responder::result(Result res)
{
	msgpack::type::nil err;
	call(res, err);
}

template <typename Result>
void responder::result(Result res, auto_zone z)
{
	msgpack::type::nil err;
	call(res, err, z);
}

template <typename Error>
void responder::error(Error err)
{
	msgpack::type::nil res;
	call(res, err);
}

template <typename Error>
void responder::error(Error err, auto_zone z)
{
	msgpack::type::nil res;
	call(res, err, z);
}

inline void responder::null()
{
	msgpack::type::nil res;
	msgpack::type::nil err;
	call(res, err);
}

template <typename Result, typename Error>
inline void responder::call(Result& res, Error& err)
{
	rpc::sbuffer buf;  // FIXME use vrefbuffer?
	rpc_response<Result&, Error> msgres(res, err, m_msgid);
	msgpack::pack(buf, msgres);
	wavy::request req(&::free, buf.data());
	wavy::write(m_fd, (char*)buf.data(), buf.size(), req);
	buf.release();
}

template <typename Result, typename Error>
inline void responder::call(Result& res, Error& err, auto_zone z)
{
	vrefbuffer* buf = z->allocate<vrefbuffer>();
	rpc_response<Result&, Error> msgres(res, err, m_msgid);
	msgpack::pack(*buf, msgres);

	wavy::request req(&mp::object_delete<msgpack::zone>, z.get());
	wavy::writev(m_fd, buf->vector(), buf->vector_size(), req);
	z.release();
}

inline void responder::send_response(const char* buf, size_t buflen, auto_zone z)
{
	wavy::request req(&mp::object_delete<msgpack::zone>, z.get());
	wavy::write(m_fd, buf, buflen, req);
	z.release();
}

inline void responder::send_responsev(const struct iovec* vb, size_t count, auto_zone z)
{
	wavy::request req(&mp::object_delete<msgpack::zone>, z.get());
	wavy::writev(m_fd, vb, count, req);
	z.release();
}


}  // namespace rpc

#endif /* rpc/responder_impl.h */

