#include "logic/srv_impl.h"
#include <mp/exception.h>
#include <sys/sendfile.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <algorithm>

#ifndef KUMO_OFFER_INITIAL_MAP_SIZE
#define KUMO_OFFER_INITIAL_MAP_SIZE 32768
#endif

namespace kumo {


void Server::init_stream(int fd)
{
	m_stream_core.reset(new mp::wavy::core());
	using namespace mp::placeholders;
	m_stream_core->listen(fd, mp::bind(
				&Server::stream_accepted, this,
				_1, _2));
}

void Server::run_stream()
{
	m_stream_core->add_thread(2);  // FIXME 2
}

void Server::stop_stream()
{
	m_stream_core->end();
}


class Server::OfferStorage::mmap_stream {
public:
	mmap_stream(int fd);
	~mmap_stream();
	void write(const void* buf, size_t len);
	size_t size() const;
private:
	size_t m_free;
	char* m_head;
	char* m_map;
	int m_fd;
	void expand_map(size_t req);
private:
	msgpack::packer<mmap_stream> m_pk;
public:
	msgpack::packer<mmap_stream>& get() { return m_pk; }
private:
	mmap_stream();
	mmap_stream(const mmap_stream&);
};

Server::OfferStorage::mmap_stream::mmap_stream(int fd) :
	m_free(KUMO_OFFER_INITIAL_MAP_SIZE),
	m_fd(fd),
	m_pk(*this)
{
	if(::ftruncate(m_fd, m_free) < 0) {
		throw mp::system_error(errno, "failed to truncate offer storage");
	}

	m_map = (char*)::mmap(NULL, m_free,
			PROT_WRITE, MAP_SHARED, m_fd, 0);
	if(m_map == MAP_FAILED) {
		throw mp::system_error(errno, "failed to mmap offer storage");
	}
	m_head = m_map;
}

Server::OfferStorage::mmap_stream::~mmap_stream()
{
	size_t used = m_head - m_map;
	size_t csize = used + m_free;
	::munmap(m_map, csize);
	//::ftruncate(m_fd, used);
}

size_t Server::OfferStorage::mmap_stream::size() const
{
	return m_head - m_map;
}

void Server::OfferStorage::mmap_stream::write(const void* buf, size_t len)
{
	if(m_free < len) { expand_map(len); }
	memcpy(m_head, buf, len);
	m_free -= len;
	m_head += len;
}

void Server::OfferStorage::mmap_stream::expand_map(size_t req)
{
	size_t used = m_head - m_map;
	size_t csize = used + m_free;
	size_t nsize = csize * 2;
	while(nsize < req) { nsize *= 2; }

	if(::ftruncate(m_fd, nsize) < 0 ) {
		throw mp::system_error(errno, "failed to resize offer storage");
	}

#ifdef __linux__
	void* tmp = ::mremap(m_map, csize, nsize, MREMAP_MAYMOVE);
	if(tmp == MAP_FAILED) {
		throw mp::system_error(errno, "failed to mremap offer storage");
	}
	m_map = (char*)tmp;

#else
	if(::munmap(m_map, csize) < 0) {
		throw mp::system_error(errno, "failed to munmap offer storage");
	}
	m_map = NULL;
	m_head = NULL;
	m_free = 0;

	m_map = (char*)::mmap(NULL, nsize,
			PROT_WRITE, MAP_SHARED, m_fd, 0);
	if(m_map == MAP_FAILED) {
		throw mp::system_error(errno, "failed to mmap");
	}

#endif
	m_head = m_map + used;
	m_free = nsize - used;
}


struct Server::SharedOfferMapComp {
	bool operator() (const SharedOfferStorage& x, const address& y) const
		{ return x->addr() < y; }
	bool operator() (const address& x, const SharedOfferStorage& y) const
		{ return x < y->addr(); }
	bool operator() (const SharedOfferStorage& x, const SharedOfferStorage& y) const
		{ return x->addr() < y->addr(); }
};


Server::OfferStorageMap::OfferStorageMap(
		const std::string& basename, ClockTime replace_time) :
	m_basename(basename),
	m_replace_time(replace_time) { }

Server::OfferStorageMap::~OfferStorageMap() { }

void Server::OfferStorageMap::add(
		const address& addr,
		const char* key, size_t keylen,
		const char* val, size_t vallen)
{
	SharedOfferMap::iterator it = find_offer_map(m_map, addr);
	if(it != m_map.end()) {
		(*it)->add(key, keylen, val, vallen);
	} else {
		SharedOfferStorage of(new OfferStorage(m_basename, addr, m_replace_time));
		//m_map.insert(it, of);  // FIXME
		m_map.push_back(of);
		std::sort(m_map.begin(), m_map.end(), SharedOfferMapComp());
		of->add(key, keylen, val, vallen);
	}
}

void Server::OfferStorageMap::commit(SharedOfferMap* dst)
{
	*dst = m_map;
}


Server::SharedOfferMap::iterator Server::find_offer_map(
		SharedOfferMap& map, const address& addr)
{
	SharedOfferMap::iterator it =
		std::lower_bound(map.begin(), map.end(),
				addr, SharedOfferMapComp());
	if(it != map.end() && (*it)->addr() == addr) {
		return it;
	} else {
		return map.end();
	}
}


int Server::OfferStorage::openfd(const std::string& basename)
{
	char* path = (char*)::malloc(basename.size()+8);
	if(!path) { throw std::bad_alloc(); }
	memcpy(path, basename.data(), basename.size());
	memcpy(path+basename.size(), "/XXXXXX", 8);  // '/XXXXXX' + 1(='\0')

	int fd = ::mkstemp(path);
	if(fd < 0) {
		::free(path);
		throw mp::system_error(errno, "failed to mktemp");
	}

	::unlink(path);
	::free(path);

	return fd;
}

Server::OfferStorage::OfferStorage(const std::string& basename,
		const address& addr, ClockTime replace_time):
	m_addr(addr),
	m_replace_time(replace_time),
	m_fd(openfd(basename)),
	m_mmap(new mmap_stream(m_fd.get()))
{
	LOG_TRACE("create OfferStorage for ",addr);
}

Server::OfferStorage::~OfferStorage() { }

void Server::OfferStorage::add(
		const char* key, size_t keylen,
		const char* val, size_t vallen)
{
	msgpack::packer<mmap_stream>& pk(m_mmap->get());
	pk.pack_array(2);
	pk.pack_raw(keylen);
	pk.pack_raw_body(key, keylen);
	pk.pack_raw(vallen);
	pk.pack_raw_body(val, vallen);
}

void Server::OfferStorage::send(int sock)
{
	size_t size = m_mmap->size();
	//m_mmap.reset(NULL);  // FIXME needed?
	while(size > 0) {
		// FIXME linux
		ssize_t rl = ::sendfile(sock, m_fd.get(), NULL, size);
		if(rl <= 0) { throw mp::system_error(errno, "offer send error"); }
		size -= rl;
	}
}


struct scopeout_close {
	scopeout_close(int fd) : m(fd) {}
	~scopeout_close() { if(m >= 0) { ::close(m); } }
	void release() { m = -1; }
private:
	int m;
	scopeout_close();
	scopeout_close(const scopeout_close&);
};


void Server::stream_accepted(int fd, int err)
try {
	LOG_TRACE("stream accepted fd(",fd,") err:",err);

	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(err));
		signal_end();
		return;
	}

	scopeout_close fdscope(fd);
	if(::fcntl(fd, F_SETFL, 0) < 0) {  // set blocking mode
		LOG_ERROR("stream connect: fcntl failed", strerror(err));
		return;
	}

	// recv init address
	address iaddr;
	{
		size_t sz = address::MAX_DUMP_SIZE+1;
		char addrbuf[sz];
		char* p = addrbuf;
		while(true) {
			ssize_t rl = ::read(fd, p, sz);
			if(rl <= 0) {
				LOG_ERROR("failed to recv init address", strerror(err));
				return;
			}
			if((size_t)rl >= sz) { break; }
			sz -= rl;
			p += rl;
		}
		iaddr = address(addrbuf+1, (uint8_t)addrbuf[0]);
	}

	// take out OfferStorage from m_offer_map
	SharedOfferStorage st;
	{
		pthread_scoped_lock oflk(m_offer_map_mutex);
		SharedOfferMap::iterator it = find_offer_map(m_offer_map, iaddr);
		if(it == m_offer_map.end()) {
			LOG_DEBUG("storage offer to ",iaddr," is already timed out");
			return;
		}
		st = *it;
		m_offer_map.erase(it);
	}

	LOG_DEBUG("send offer storage to ",iaddr);
	st->send(fd);
	LOG_DEBUG("finish to send offer storage to ",iaddr);

	pthread_scoped_lock relk(m_replacing_mutex);
	replace_offer_finished(st->replace_time(), relk);

} catch (std::exception& e) {
	LOG_WARN("failed to send offer storage: ",e.what());
	throw;
} catch (...) {
	LOG_WARN("failed to send offer storage: unknown error");
	throw;
}


void Server::stream_connected(int fd, int err)
try {
	LOG_TRACE("stream connected fd(",fd,") err:",err);
	if(fd < 0) {
		LOG_ERROR("stream connect failed", strerror(err));
		return;
	}

	scopeout_close fdscope(fd);

	if(::fcntl(fd, F_SETFL, 0) < 0) {  // set blocking mode
		LOG_ERROR("stream connect: fcntl failed", strerror(err));
		return;
	}

	// send init address
	{
		size_t sz = address::MAX_DUMP_SIZE+1;
		char addrbuf[sz];
		::memset(addrbuf, 0, sz);
		addrbuf[0] = (uint8_t)addr().dump_size();
		::memcpy(addrbuf+1, addr().dump(), addr().dump_size());
		const char* p = addrbuf;
		while(true) {
			ssize_t rl = ::write(fd, p, sz);
			if(rl <= 0) {
				LOG_ERROR("failed to send init address", strerror(err));
				return;
			}
			if((size_t)rl >= sz) { break; }
			sz -= rl;
			p += rl;
		}
	}

	mp::set_nonblock(fd);

	m_stream_core->add<OfferStreamHandler>(fd, this);
	fdscope.release();

} catch (std::exception& e) {
	LOG_WARN("failed to receve offer storage: ",e.what());
	throw;
} catch (...) {
	LOG_WARN("failed to receve offer storage: unknown error");
	throw;
}

class Server::OfferStreamHandler : public rpc::connection<OfferStreamHandler> {
public:
	OfferStreamHandler(int fd, Server* srv) :
		rpc::connection<OfferStreamHandler>(fd), m_srv(srv) { }
	~OfferStreamHandler() { }
	void submit_message(msgobj msg, auto_zone& z);
private:
	msgpack::unpacker m_pac;
	Server* m_srv;
	OfferStreamHandler();
	OfferStreamHandler(const OfferStreamHandler&);
};

void Server::OfferStreamHandler::submit_message(msgobj msg, auto_zone& z)
{
	msgpack::type::tuple<protocol::type::DBKey, protocol::type::DBValue> kv(msg);
	protocol::type::DBKey key = kv.get<0>();
	protocol::type::DBValue val = kv.get<1>();

	// FIXME updatev
	m_srv->m_db.update(
			key.raw_data(), key.raw_size(),
			val.raw_data(), val.raw_size());

	// update() returns false means that key is overwritten while replicating.
}


}  // namespace kumo

