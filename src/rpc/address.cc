//
// kumofs
//
// Copyright (C) 2009 Etolabo Corp.
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
#include "rpc/address.h"
#include <stdexcept>
#include <string.h>

namespace rpc {


address::address(const struct sockaddr_in& addr)
{
#ifdef KUMO_IPV6
	m_serial_length = 6;
	memcpy(&m_serial_address[0], &addr.sin_port, 2);
	memcpy(&m_serial_address[2], &addr.sin_addr.s_addr, 4);
#else
	m_serial = addr.sin_addr.s_addr;
	m_serial <<= 16;
	m_serial |= addr.sin_port;
#endif
}

#ifdef KUMO_IPV6
address::address(const struct sockaddr_in6& addr)
{
	m_serial_length = 22;
	memcpy(&m_serial_address[0], &addr.sin6_port, 2);
	memcpy(&m_serial_address[2], addr.sin6_addr.s6_addr, 16);
	memcpy(&m_serial_address[18], &addr.sin6_scope_id, 4);
}
#endif

address::address(const char* ptr, unsigned int len)
{
#ifdef KUMO_IPV6
	if(len != 6 && len != 22) {
		throw std::runtime_error("unknown address type");
	}

	memcpy(m_serial_address, ptr, len);
	m_serial_length = len;

#else
	if(len != 6) {
		throw std::runtime_error("unknown address type");
	}

	m_serial = 0;
	memcpy(&m_serial, ptr, len);  // FIXME
#endif
}


void address::getaddr(sockaddr* addrbuf) const
{
#ifdef KUMO_IPV6
	if(m_serial_length == 6) {
		sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(addrbuf);

		memset(addr, 0, sizeof(sockaddr_in));
		addr->sin_family = AF_INET;
		addr->sin_port = raw_port();
		addr->sin_addr.s_addr = *((uint32_t*)&m_serial_address[2]);

	} else {
		sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(addrbuf);

		memset(addr, 0, sizeof(sockaddr_in6));
		addr->sin6_family = AF_INET6;
		addr->sin6_port = raw_port();
		memcpy(addr->sin6_addr.s6_addr, &m_serial_address[2], 16);
		addr->sin6_scope_id = *((uint32_t*)&m_serial_address[18]);
	}

#else
	sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(addrbuf);

	memset(addr, 0, sizeof(sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = raw_port();
	addr->sin_addr.s_addr = (uint32_t)(m_serial >> 16);
#endif
}


std::ostream& operator<< (std::ostream& stream, const address& addr)
{
#ifdef KUMO_IPV6
	if(addr.m_serial_length == 6) {
		uint32_t sa = *(uint32_t*)&addr.m_serial_address[2];
		char buf[16];
		return stream << ::inet_ntop(AF_INET, &sa, buf, sizeof(buf)) << ':' << ntohs(addr.raw_port());
	} else {
		unsigned char sa[16];
		char buf[41];
		memcpy(sa, &addr.m_serial_address[2], sizeof(sa));
		return stream << '[' << ::inet_ntop(AF_INET6, sa, buf, sizeof(buf)) << "]:" << ntohs(addr.raw_port());
	}
#else
	uint32_t sa = (uint32_t)(addr.m_serial >> 16);
	char buf[16];
	return stream << ::inet_ntop(AF_INET, &sa, buf, sizeof(buf)) << ':' << ntohs(addr.raw_port());
#endif
}


}  // namespace rpc

