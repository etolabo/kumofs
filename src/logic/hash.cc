#include "logic/hash.h"
#include "log/mlogger.h"
#include <openssl/sha.h>

namespace kumo {

static const size_t HASHSPACE_VIRTUAL_NODE_NUMBER = 128;


HashSpace::HashSpace(ClockTime clocktime) :
	m_timestamp(clocktime) {}

HashSpace::~HashSpace() {}


/*
class HashFunction {
public:
	HashFunction() { SHA1_Init(&m_ctx); }
	uint64_t operator() (const char* data, unsigned long datalen)
	{
		SHA1_Update(&m_ctx, data, datalen);
		SHA1_Final(m_buf, &m_ctx);
		return *(uint64_t*)m_buf;  // FIXME endian
	}
private:
	SHA_CTX m_ctx;
	unsigned char m_buf[SHA_DIGEST_LENGTH];
};
static HashFunction HashFunction_;
*/

uint64_t HashSpace::hash(const char* data, unsigned long len)
{
	// FIXME thread-safety with thread local storage
	//return HashFunction_(data, len);
	unsigned char buf[SHA_DIGEST_LENGTH];
	SHA1((unsigned const char*)data, len, buf);
	return *(uint64_t*)buf;  // FIXME endian?
}

void HashSpace::add_server(ClockTime clocktime, const address& addr)
{
	m_timestamp = clocktime;
	m_nodes.push_back( node(addr,true) );
	add_virtual_nodes(m_nodes.back());
	std::stable_sort(m_hashspace.begin(), m_hashspace.end());
}

bool HashSpace::remove_server(ClockTime clocktime, const address& addr)
{
	nodes_t::iterator it =
		std::find_if(m_nodes.begin(), m_nodes.end(),
				node_address_equal(addr));
	if(it != m_nodes.end()) {
		m_nodes.erase(it);
		m_timestamp = clocktime;
		rehash();
		return true;
	}
	return false;
}

bool HashSpace::fault_server(ClockTime clocktime, const address& addr)
{
	nodes_t::iterator it =
		std::find_if(m_nodes.begin(), m_nodes.end(),
				node_address_equal(addr));
	if(it != m_nodes.end()) {
		it->fault();
		m_timestamp = clocktime;
		return true;
	}
	return false;
}

bool HashSpace::recover_server(ClockTime clocktime, const address& addr)
{
	nodes_t::iterator it =
		std::find_if(m_nodes.begin(), m_nodes.end(),
				node_address_equal(addr));
	if(it != m_nodes.end()) {
		it->recover();
		m_timestamp = clocktime;
		return true;
	}
	return false;
}

bool HashSpace::remove_fault_servers(ClockTime clocktime)
{
	bool ret = false;
	for(nodes_t::iterator it(m_nodes.begin()); it != m_nodes.end(); ) {
		if(!it->is_active()) {
			ret = true;
			it = m_nodes.erase(it);
		} else {
			++it;
		}
	}
	if(ret) {
		m_timestamp = clocktime;
		rehash();
		return true;
	}
	return false;
}

void HashSpace::add_virtual_nodes(const node& n)
{
	uint64_t x = HashSpace::hash(n.addr().dump(), n.addr().dump_size());
	m_hashspace.push_back( virtual_node(x, n) );
	for(size_t i=1; i < HASHSPACE_VIRTUAL_NODE_NUMBER; ++i) {
		// FIXME use another hash function?
		x = HashSpace::hash((const char*)&x, sizeof(uint64_t));
		m_hashspace.push_back( virtual_node(x, n) );
	}
}

void HashSpace::rehash()
{
	m_hashspace.clear();
	for(nodes_t::const_iterator it(m_nodes.begin()), it_end(m_nodes.end());
			it != it_end; ++it) {
		add_virtual_nodes(*it);
	}
	std::stable_sort(m_hashspace.begin(), m_hashspace.end());

	for(hashspace_t::const_iterator x(m_hashspace.begin()), x_end(m_hashspace.end());
			x != x_end; ++x) {
		LOG_TRACE("virtual node dump: ",x->hash(),":",x->real());
	}
}


}  // namespace kumo

