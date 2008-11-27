#ifndef RPC_NODE_ADDRESS_H__
#define RPC_NODE_ADDRESS_H__

#include "rpc/address.h"
#include <typeinfo>

namespace rpc {


template <typename IMPL>
struct node_address_tag { };

struct node_address : protected address {
	template <typename IMPL>
	node_address(node_address_tag<IMPL>, const address& addr);

	unsigned int dump_size() const;
	const char* dump() const;
	bool connectable() const;

	socklen_t addrlen() const;
	void getaddr(sockaddr* addrbuf) const;

	bool operator== (const node_address& addr) const;
	bool operator!= (const node_address& addr) const;
	bool operator<  (const node_address& addr) const;
	bool operator>  (const node_address& addr) const;

private:
	const std::type_info& m_type;
	const address m_addr;
};


template <typename IMPL>
node_address::node_address(node_address_tag<IMPL>, const address& addr) :
	address(addr), m_type(typeid(IMPL)) { }

inline unsigned int node_address::dump_size() const
	{ return address::dump_size(); }

inline const char* node_address::dump() const
	{ return address::dump(); }

inline bool node_address::connectable() const
	{ return address::connectable(); }

inline socklen_t node_address::addrlen() const
	{ return address::addrlen(); }

inline void node_address::getaddr(sockaddr* addrbuf) const
	{ address::getaddr(addrbuf); }

inline bool node_address::operator== (const node_address& addr) const
{
	return address::operator==(static_cast<const address&>(addr)) &&
		m_type == addr.m_type;
}

inline bool node_address::operator!= (const node_address& addr) const
{
	return !(*this == addr);
}

inline bool node_address::operator< (const node_address& addr) const
{
	if(m_type == addr.m_type) {
		return address::operator<(static_cast<const address&>(addr));
	} else {
		return m_type.before(addr.m_type);
	}
}

inline bool node_address::operator> (const node_address& addr) const
{
	if(m_type == addr.m_type) {
		return address::operator>(static_cast<const address&>(addr));
	} else {
		return addr.m_type.before(m_type);
	}
}


}  // namespace rpc

#endif /* rpc/node_address.h */

