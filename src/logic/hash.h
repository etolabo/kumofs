#ifndef LOGIC_HASH_H__
#define LOGIC_HASH_H__

#include "rpc/address.h"
#include "logic/clock.h"
#include <vector>
#include <algorithm>
#include <ostream>

namespace kumo {


using rpc::address;


class HashSpace {
public:
	class Seed;

	HashSpace(ClockTime clocktime = ClockTime(0,0));
	HashSpace(const Seed& seed);
	~HashSpace();

public:
	class node {
	public:
		node() {}
		node(const address& addr, bool active) : m_addr(addr), m_active(active) {}
	public:
		const address& addr() const { return m_addr; }
		bool is_active()      const { return m_active; }
		void fault()   { m_active = false; }
		void recover() { m_active = true; }
		bool operator== (const node& other) const;
	private:
		address m_addr;
		bool m_active;
	};

	struct node_address_equal;

private:
	struct virtual_node {
		virtual_node(uint64_t h) : m_hash(h) {}
		virtual_node(uint64_t h, const node& r) : m_hash(h), m_real(r) {}
	public:
		uint64_t hash()       const { return m_hash; }
		const node& real()    const { return m_real; }
		bool operator< (const virtual_node& other) const
		{
			return m_hash < other.m_hash;
		}
	private:
		uint64_t m_hash;
		node m_real;
	};

	// sorted vector
	typedef std::vector<virtual_node> hashspace_t;
	hashspace_t m_hashspace;

	typedef std::vector<node> nodes_t;
	nodes_t m_nodes;

	ClockTime m_timestamp;

public:
	class iterator;

	iterator find(uint64_t h) const;

	size_t active_node_count() const;
	void get_active_nodes(std::vector<address>& result) const;

public:
	void add_server(ClockTime clocktime, const address& addr);
	bool remove_server(ClockTime clocktime, const address& addr);
	bool fault_server(ClockTime clocktime, const address& addr);
	bool recover_server(ClockTime clocktime, const address& addr);
	bool remove_fault_servers(ClockTime clocktime);

	bool empty() const;

	ClockTime clocktime() const
		{ return m_timestamp; }

	// compare nodes (clocktime is ignored)
	bool operator== (const HashSpace& other) const
		{ return m_nodes == other.m_nodes; }

	void nodes_diff(const HashSpace& other, std::vector<address>& result) const;

	bool server_is_include(const address& addr) const;
	bool server_is_active(const address& addr) const;
	bool server_is_fault(const address& addr) const;

private:
	void add_virtual_nodes(const node& n);
	void rehash();

public:
	static uint64_t hash(const char* data, unsigned long len);

public:
	friend class Seed;

	// compare nodes (clocktime is ignored)
	bool operator== (const Seed& other) const;
};


class HashSpace::iterator {
public:
	iterator(const hashspace_t& hs, hashspace_t::const_iterator it) :
		m_it(it), m_hashspace(hs) {}
	~iterator() {}
public:
	iterator& operator++ ()
	{
		++m_it;
		if(m_it == m_hashspace.end()) {
			m_it = m_hashspace.begin();
		}
		return *this;
	}

	bool operator== (const iterator& it) const
	{
		return m_it == it.m_it;
	}

	bool operator!= (const iterator& it) const
	{
		return !(*this == it);
	}

	const node& operator* () const
	{
		return m_it->real();
	}

	const node* operator-> () const
	{
		return &m_it->real();
	}

private:
	hashspace_t::const_iterator m_it;
	const hashspace_t& m_hashspace;
};


inline bool HashSpace::node::operator== (const node& other) const
{
	return m_active == other.m_active && m_addr == other.m_addr;
}

struct HashSpace::node_address_equal {
	node_address_equal(const address& a) : m(a) {}
	bool operator() (const HashSpace::node& other) const
	{
		return m == other.addr();
	}
private:
	const address& m;
};


inline std::ostream& operator<< (std::ostream& stream, const HashSpace::node& n)
{
	return stream << n.addr() << '(' << (n.is_active() ? "active" : "fault") << ')';
}

inline HashSpace::node& operator>> (msgpack::object o, HashSpace::node& v)
{
	using namespace msgpack;
	if(o.type != type::RAW) { throw type_error(); }
	address addr(o.via.raw.ptr+1, o.via.raw.size-1);  // sie is checked in address::address
	bool active = o.via.raw.ptr[0] != 0;
	v = HashSpace::node(addr, active);
	return v;
}

template <typename Stream>
inline msgpack::packer<Stream>& operator<< (msgpack::packer<Stream>& o, const HashSpace::node& v)
{
	using namespace msgpack;
	o.pack_raw(1 + v.addr().dump_size());
	char a = v.is_active() ? 1 : 0;
	o.pack_raw_body(&a, 1);
	o.pack_raw_body(v.addr().dump(), v.addr().dump_size());
	return o;
}


class HashSpace::Seed : public msgpack::define<
		msgpack::type::tuple<nodes_t, ClockTime> > {
public:
	Seed() { }
	Seed(HashSpace& hs) :
		define_type(msgpack_type( hs.m_nodes, hs.m_timestamp )) {}
	const nodes_t& nodes()     const { return get<0>(); }
	ClockTime      clocktime() const { return get<1>(); }
	bool           empty()     const { return get<0>().empty(); }
};

inline HashSpace::HashSpace(const Seed& seed) :
	m_nodes(seed.nodes()), m_timestamp(seed.clocktime())
{
	rehash();
}

inline bool HashSpace::operator== (const Seed& other) const
{
	return m_nodes == other.nodes();
}


inline HashSpace::iterator HashSpace::find(uint64_t h) const
{
	hashspace_t::const_iterator it(
			std::lower_bound(m_hashspace.begin(), m_hashspace.end(), virtual_node(h))
			);
	if(it == m_hashspace.end()) {
		return iterator(m_hashspace, m_hashspace.begin());
	} else {
		return iterator(m_hashspace, it);
	}
}

inline bool HashSpace::empty() const
{
	for(nodes_t::const_iterator it(m_nodes.begin()), it_end(m_nodes.end());
			it != it_end; ++it) {
		if(it->is_active()) { return false; }
	}
	return true;
}

inline size_t HashSpace::active_node_count() const
{
	size_t n = 0;
	for(nodes_t::const_iterator it(m_nodes.begin()), it_end(m_nodes.end());
			it != it_end; ++it) {
		if(it->is_active()) { ++n; }
	}
	return n;
}

inline void HashSpace::get_active_nodes(std::vector<address>& result) const
{
	for(nodes_t::const_iterator it(m_nodes.begin()), it_end(m_nodes.end());
			it != it_end; ++it) {
		if(it->is_active()) { result.push_back(it->addr()); }
	}
}

inline void HashSpace::nodes_diff(const HashSpace& other, std::vector<address>& result) const
{
	for(nodes_t::const_iterator it(m_nodes.begin()), it_end(m_nodes.end());
			it != it_end; ++it) {
		if(std::find_if(other.m_nodes.begin(), other.m_nodes.end(),
					node_address_equal(it->addr())) == other.m_nodes.end()) {
			result.push_back(it->addr());
		}
	}
}

inline bool HashSpace::server_is_include(const address& addr) const
{
	return std::find_if(
			m_nodes.begin(), m_nodes.end(),
			node_address_equal(addr)) != m_nodes.end();
}

inline bool HashSpace::server_is_active(const address& addr) const
{
	nodes_t::const_iterator it = std::find_if(
			m_nodes.begin(), m_nodes.end(),
			node_address_equal(addr));
	if(it != m_nodes.end() && it->is_active()) {
		return true;
	}
	return false;
}

inline bool HashSpace::server_is_fault(const address& addr) const
{
	nodes_t::const_iterator it = std::find_if(
			m_nodes.begin(), m_nodes.end(),
			node_address_equal(addr));
	if(it != m_nodes.end() && !it->is_active()) {
		return true;
	}
	return false;
}


}  // namespace kumo

#endif /* logic/hash.h */

