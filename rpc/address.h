#ifndef RPC_ADDRESS_H__
#define RPC_ADDRESS_H__

#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <msgpack.hpp>

namespace rpc {


class address {
public:
	address();
	address(const struct sockaddr_in& addr);
	address(const struct sockaddr_in6& addr);
	address(const char* ptr, unsigned int len);
//	address(const address& o);

public:
	unsigned int dump_size() const;
	const char* dump() const;

	static const unsigned int MAX_DUMP_SIZE = 22;

	bool connectable() const;

private:
	// +--+----+
	// | 2|  4 |
	// +--+----+
	// port network byte order
	//    IPv4 address
	//
	// +--+----------------+----+
	// | 2|       16       |  4 |
	// +--+----------------+----+
	// port network byte order
	//    IPv6 address
	//                     scope id
	char m_serial_address[22];
	unsigned int m_serial_length;  // 6 or 22

public:
	socklen_t addrlen() const;
	void getaddr(sockaddr* addrbuf) const;

private:
	uint16_t raw_port() const;

public:
	bool operator== (const address& addr) const;
	bool operator!= (const address& addr) const;
	bool operator<  (const address& addr) const;
	bool operator>  (const address& addr) const;

	friend std::ostream& operator<< (std::ostream& stream, const address& addr);
};

std::ostream& operator<< (std::ostream& stream, const address& addr);


inline address::address() :
	m_serial_length(0)
{
	*((uint16_t*)&m_serial_address[0]) = 0;
}

//inline address::address(const address& o) :
//	m_serial_length(o.m_serial_length)
//{
//	memcpy(m_serial_address, o.m_serial_address, m_serial_length);
//}

inline unsigned int address::dump_size() const
{ return m_serial_length; }

inline const char* address::dump() const
{ return m_serial_address; }	

inline bool address::connectable() const
{ return raw_port() != 0; }

inline socklen_t address::addrlen() const
{ return m_serial_length == 6 ? sizeof(sockaddr_in) : sizeof(sockaddr_in6); }

inline bool address::operator== (const address& addr) const
{
	return m_serial_length == addr.m_serial_length &&
		memcmp(m_serial_address, addr.m_serial_address, m_serial_length) == 0;
}

inline bool address::operator!= (const address& addr) const
{
	return !(*this == addr);
}

inline bool address::operator< (const address& addr) const
{
	if(m_serial_length == addr.m_serial_length) {
		return memcmp(m_serial_address, addr.m_serial_address, m_serial_length) < 0;
	} else {
		return m_serial_length < addr.m_serial_length;
	}
}

inline bool address::operator> (const address& addr) const
{
	if(m_serial_length == addr.m_serial_length) {
		return memcmp(m_serial_address, addr.m_serial_address, m_serial_length) > 0;
	} else {
		return m_serial_length > addr.m_serial_length;
	}
}

inline uint16_t address::raw_port() const
{ return *((uint16_t*)&m_serial_address[0]); }


#ifdef MSGPACK_OBJECT_HPP__
inline address& operator>> (msgpack::object o, address& v)
{
	using namespace msgpack;
	if(o.type != type::RAW) { throw type_error(); }
	v = address(o.via.ref.ptr, o.via.ref.size);
	return v;
}

template <typename Stream>
inline msgpack::packer<Stream>& operator<< (msgpack::packer<Stream>& o, const address& v)
{
	using namespace msgpack;
	o.pack_raw(v.dump_size());
	o.pack_raw_body(v.dump(), v.dump_size());
	return o;
}
#endif


}  // namespace rpc


#endif /* rpc/address.h */

