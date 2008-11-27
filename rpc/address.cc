#include "rpc/address.h"
#include <stdexcept>
#include <string.h>

namespace rpc {


address::address(const struct sockaddr_in& addr)
{
	m_serial_length = 6;
	memcpy(&m_serial_address[0], &addr.sin_port, 2);
	memcpy(&m_serial_address[2], &addr.sin_addr.s_addr, 4);
}

address::address(const struct sockaddr_in6& addr)
{
	m_serial_length = 22;
	memcpy(&m_serial_address[0], &addr.sin6_port, 2);
	memcpy(&m_serial_address[2], addr.sin6_addr.s6_addr, 16);
	memcpy(&m_serial_address[18], &addr.sin6_scope_id, 4);
}

address::address(const char* ptr, unsigned int len)
{
	if(len != 6 && len != 22) { throw std::runtime_error("unknown address type"); }
	memcpy(m_serial_address, ptr, len);
	m_serial_length = len;
}


void address::getaddr(sockaddr* addrbuf) const
{
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
}


std::ostream& operator<< (std::ostream& stream, const address& addr)
{
	/*
	char addrbuf[addr.addrlen()];
	struct sockaddr* const sa = (sockaddr*)addrbuf;
	addr.getaddr(sa);
	if(sa->sa_family == AF_INET) {
		char buf[16];
		struct sockaddr_in* const sin = (sockaddr_in*)sa;
		return stream << ::inet_ntop(AF_INET, &sin->sin_addr.s_addr, buf, sizeof(buf)) << ':' << ntohs(sin->sin_port);
	} else {
		char buf[41];
		struct sockaddr_in6* const sin = (sockaddr_in6*)sa;
		return stream << '[' << ::inet_ntop(AF_INET6, sin->sin6_addr.s6_addr, buf, sizeof(buf)) << "]:" << ntohs(sin->sin6_port);
	}
	*/
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
}


}  // namespace rpc

