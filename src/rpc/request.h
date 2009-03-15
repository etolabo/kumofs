#ifndef RPC_REQUEST_H__
#define RPC_REQUEST_H__

namespace rpc {


template <typename Parameter, typename Session = typename Parameter::session_type>
class request;


template <typename Parameter>
class request<Parameter, basic_session> {
public:
	request(basic_shared_session from, msgobj param) :
		m_from(from)
	{
		param.convert(&m_param);
	}

public:
	Parameter& param()
	{
		return m_param;
	}

	const Parameter& param() const
	{
		return m_param;
	}

	basic_shared_session& session()
	{
		return m_from;
	}

private:
	Parameter m_param;
	basic_shared_session m_from;

private:
	request();
};



}  // namespace rpc

#endif /* rpc/request.h */

