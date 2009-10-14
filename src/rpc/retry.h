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
#ifndef RPC_RETRY_H__
#define RPC_RETRY_H__

namespace rpc {


template <typename Parameter>
class retry {
public:
	retry(Parameter param) :
		m_limit(0), m_param(param) { }

	void set_callback(rpc::callback_t callback)
	{
		m_callbck = callback;
	}

public:
	bool retry_incr(unsigned short limit)
	{
		return ++m_limit <= limit;
	}

	template <typename Session>
	void call(Session s, rpc::shared_zone& life, unsigned short timeout_steps = 10)
	{
		s->call(m_param, life, m_callbck, timeout_steps);
	}

	unsigned short num_retried() const
	{
		return m_limit;
	}

	Parameter& param()
	{
		return m_param;
	}

	const Parameter& param() const
	{
		return m_param;
	}

private:
	unsigned short m_limit;
	Parameter m_param;
	rpc::callback_t m_callbck;

private:
	retry();
	retry(const retry&);
};


}  // namespace rpc

#endif /* rpc/retry.h */

