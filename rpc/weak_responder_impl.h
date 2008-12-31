#ifndef RPC_WEAK_RESPONDER_H__
#define RPC_WEAK_RESPONDER_H__

#include <mp/object_callback.h>

namespace rpc {


inline weak_responder::weak_responder(basic_weak_session s, msgid_t msgid) :
	m_session(s), m_msgid(msgid) { }

inline weak_responder::~weak_responder() { }


template <typename Result>
void weak_responder::result(Result res)
{
	LOG_TRACE("send response data with Success id=",m_msgid);
	msgpack::type::nil err;
	call(res, err);
}

template <typename Result>
void weak_responder::result(Result res, auto_zone& z)
{
	LOG_TRACE("send response data with Success id=",m_msgid);
	msgpack::type::nil err;
	call(res, err, z);
}

template <typename Result>
void weak_responder::result(Result res, shared_zone& life)
{
	LOG_TRACE("send response data with Success id=",m_msgid);
	msgpack::type::nil err;
	call(res, err, life);
}

template <typename Error>
void weak_responder::error(Error err)
{
	LOG_TRACE("send response data with Error id=",m_msgid);
	msgpack::type::nil res;
	call(res, err);
}

template <typename Error>
void weak_responder::error(Error err, auto_zone& z)
{
	LOG_TRACE("send response data with Error id=",m_msgid);
	msgpack::type::nil res;
	call(res, err, z);
}

template <typename Error>
void weak_responder::error(Error err, shared_zone& life)
{
	LOG_TRACE("send response data with Error id=",m_msgid);
	msgpack::type::nil res;
	call(res, err, life);
}

inline void weak_responder::null()
{
	msgpack::type::nil res;
	msgpack::type::nil err;
	call(res, err);
}


namespace detail {
	template <typename ZoneType>
	struct zone_keeper {
		zone_keeper(ZoneType& z) : m(z) { }
		~zone_keeper() { }
		vrefbuffer buf;
	private:
		ZoneType m;
		zone_keeper();
		zone_keeper(const zone_keeper&);
	};
}

template <typename Result, typename Error>
void weak_responder::call(Result& res, Error& err)
{
	rpc::sbuffer buf;  // FIXME use vrefbuffer?
	rpc_response<Result, Error> msgres(res, err, m_msgid);
	msgpack::pack(buf, msgres);

	basic_shared_session s(m_session.lock());
	if(!s) { throw std::runtime_error("lost session"); }

	s->send_data((const char*)buf.data(), buf.size(),
			&::free,
			reinterpret_cast<void*>(buf.data()));
	buf.release();
}

template <typename Result, typename Error>
inline void weak_responder::call(Result& res, Error& err, auto_zone& z)
{
	call_impl<Result, Error>(res, err, z);
}

template <typename Result, typename Error>
inline void weak_responder::call(Result& res, Error& err, shared_zone& z)
{
	call_impl<Result, Error>(res, err, z);
}

template <typename Result, typename Error, typename ZoneType>
void weak_responder::call_impl(Result& res, Error& err, ZoneType& life)
{
	std::auto_ptr<detail::zone_keeper<ZoneType> > zk(new detail::zone_keeper<ZoneType>(life));

	rpc_response<Result&, Error> msgres(res, err, m_msgid);
	msgpack::pack(zk->buf, msgres);

	basic_shared_session s(m_session.lock());
	if(!s) { throw std::runtime_error("lost session"); }

	s->send_datav(&zk->buf,
			&mp::object_delete<detail::zone_keeper<ZoneType> >,
			reinterpret_cast<void*>(zk.get()));
	zk.release();
}


}  // namespace rpc

#endif /* rpc/weak_responder.h */

