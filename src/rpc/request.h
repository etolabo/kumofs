//
// kumofs
//
// Copyright (C) 2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
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

