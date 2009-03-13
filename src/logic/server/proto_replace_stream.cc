#include "server/framework.h"
#include "server/proto_replace_stream.h"
#include <mp/exception.h>
#include <mp/utility.h>
#include <sys/sendfile.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <algorithm>
#include <zlib.h>

#ifndef KUMO_OFFER_INITIAL_MAP_SIZE
#define KUMO_OFFER_INITIAL_MAP_SIZE 32768
#endif

namespace kumo {
namespace server {


proto_replace_stream::proto_replace_stream(address stream_addr) :
	m_stream_addr(stream_addr)
{ }

proto_replace_stream::~proto_replace_stream() { }

void proto_replace_stream::init_stream(int fd)
{
	m_stream_core.reset(new mp::wavy::core());
	using namespace mp::placeholders;
	m_stream_core->listen(fd, mp::bind(
				&proto_replace_stream::stream_accepted, this,
				_1, _2));
}

void proto_replace_stream::run_stream()
{
	m_stream_core->add_thread(2);  // FIXME 2
}

void proto_replace_stream::stop_stream()
{
	m_stream_core->end();
}


class proto_replace_stream::stream_accumulator {
public:
	stream_accumulator(const std::string& basename,
			const address& addr, ClockTime replace_time);
	~stream_accumulator();
public:
	void add(const char* key, size_t keylen,
			const char* val, size_t vallen);
	void send(int sock);

	const address& addr() const { return m_addr; }
	ClockTime replace_time() const { return m_replace_time; }
private:
	address m_addr;
	ClockTime m_replace_time;

	struct scoped_fd {
		scoped_fd(int fd) : m(fd) { }
		~scoped_fd() { ::close(m); }
		int get() { return m; }
	private:
		int m;
		scoped_fd();
		scoped_fd(const scoped_fd&);
	};
	static int openfd(const std::string& basename);
	scoped_fd m_fd;

	class mmap_stream;
	std::auto_ptr<mmap_stream> m_mmap;
private:
	stream_accumulator();
	stream_accumulator(const stream_accumulator&);
};


RPC_IMPL(proto_replace_stream, ReplaceOffer_1, req, z, response)
try {
	address stream_addr = req.node()->addr();
	stream_addr.set_port(req.param().port);
	char addrbuf[stream_addr.addrlen()];
	stream_addr.getaddr((sockaddr*)addrbuf);

	using namespace mp::placeholders;
	m_stream_core->connect(
			PF_INET, SOCK_STREAM, 0,
			(sockaddr*)addrbuf, sizeof(addrbuf),
			net->connect_timeout_msec(),
			mp::bind(&proto_replace_stream::stream_connected, this, _1, _2));

	// Note: don't return any result
	LOG_TRACE("connect replace offer to ",req.node()->addr()," with stream port ",req.param().port);
}
RPC_CATCH(ReplaceDeleteStart, response)


void proto_replace_stream::send_offer(proto_replace_stream::offer_storage& offer, ClockTime replace_time)
{
	pthread_scoped_lock oflk(m_offer_map_mutex);
	offer.commit(&m_accum_set);

	pthread_scoped_lock relk(net->scope_proto_replace().state_mutex());

	for(accum_set_t::iterator it(m_accum_set.begin()),
			it_end(m_accum_set.end()); it != it_end; ++it) {
		const address& addr( (*it)->addr() );

		LOG_DEBUG("send offer to ",(*it)->addr());
		shared_zone nullz;
		proto_replace_stream::ReplaceOffer_1 param(m_stream_addr.port());

		using namespace mp::placeholders;
		net->get_node(addr)->call(param, nullz,
				BIND_RESPONSE(proto_replace_stream, ReplaceOffer_1, replace_time, addr), 160);  // FIXME 160

		net->scope_proto_replace().replace_offer_push(replace_time, relk);
	}
}


RPC_REPLY_IMPL(proto_replace_stream, ReplaceOffer_1, from, res, err, life,
		ClockTime replace_time, address addr)
{
	LOG_TRACE("ResReplaceOffer from ",addr," res:",res," err:",err);
	// Note: this request always timed out

	pthread_scoped_lock oflk(m_offer_map_mutex);

	accum_set_t::iterator it = accum_set_find(m_accum_set, addr);
	if(it == m_accum_set.end()) {
		return;
	}

	m_accum_set.erase(it);
}



class proto_replace_stream::stream_accumulator::mmap_stream {
public:
	mmap_stream(int fd);
	~mmap_stream();
	size_t size() const;

	void write(const void* buf, size_t len);
	void flush();

private:
	z_stream m_z;

	char* m_map;
	int m_fd;
	void expand_map(size_t req);

private:
	msgpack::packer<mmap_stream> m_mpk;

public:
	msgpack::packer<mmap_stream>& get() { return m_mpk; }

private:
	mmap_stream();
	mmap_stream(const mmap_stream&);
};

proto_replace_stream::stream_accumulator::mmap_stream::mmap_stream(int fd) :
	m_fd(fd),
	m_mpk(*this)
{
	m_z.zalloc = Z_NULL;
	m_z.zfree = Z_NULL;
	m_z.opaque = Z_NULL;
	if(deflateInit(&m_z, Z_DEFAULT_COMPRESSION) != Z_OK) {
		throw std::runtime_error(m_z.msg);
	}

	if(::ftruncate(m_fd, KUMO_OFFER_INITIAL_MAP_SIZE) < 0) {
		deflateEnd(&m_z);
		throw mp::system_error(errno, "failed to truncate offer storage");
	}

	m_map = (char*)::mmap(NULL, KUMO_OFFER_INITIAL_MAP_SIZE,
			PROT_WRITE, MAP_SHARED, m_fd, 0);
	if(m_map == MAP_FAILED) {
		deflateEnd(&m_z);
		throw mp::system_error(errno, "failed to mmap offer storage");
	}

	m_z.avail_out = KUMO_OFFER_INITIAL_MAP_SIZE;
	m_z.next_out = (Bytef*)m_map;
}

proto_replace_stream::stream_accumulator::mmap_stream::~mmap_stream()
{
	size_t used = (char*)m_z.next_out - m_map;
	size_t csize = used + m_z.avail_out;
	::munmap(m_map, csize);
	//::ftruncate(m_fd, used);
	deflateEnd(&m_z);
}

size_t proto_replace_stream::stream_accumulator::mmap_stream::size() const
{
	return (char*)m_z.next_out - m_map;
}

void proto_replace_stream::stream_accumulator::mmap_stream::write(const void* buf, size_t len)
{
	m_z.next_in = (Bytef*)buf;
	m_z.avail_in = len;

	while(true) {
		if(m_z.avail_out < RPC_BUFFER_RESERVATION_SIZE) { // FIXME size
			expand_map(KUMO_OFFER_INITIAL_MAP_SIZE); // FIXME size
		}

		if(deflate(&m_z, Z_NO_FLUSH) != Z_OK) {
			throw std::runtime_error("deflate failed");
		}

		if(m_z.avail_in == 0) {
			break;
		}
	}
}

void proto_replace_stream::stream_accumulator::mmap_stream::flush()
{
	while(true) {
		switch(deflate(&m_z, Z_FINISH)) {

		case Z_STREAM_END:
			return;

		case Z_OK:
			break;

		default:
			throw std::runtime_error("deflate flush failed");
		}

		expand_map(m_z.avail_in);
	}
}

void proto_replace_stream::stream_accumulator::mmap_stream::expand_map(size_t req)
{
	size_t used = (char*)m_z.next_out - m_map;
	size_t csize = used + m_z.avail_out;
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
	m_z.next_out = NULL;
	m_z.avail_out = 0;

	m_map = (char*)::mmap(NULL, nsize,
			PROT_WRITE, MAP_SHARED, m_fd, 0);
	if(m_map == MAP_FAILED) {
		throw mp::system_error(errno, "failed to mmap");
	}

#endif
	m_z.next_out = (Bytef*)(m_map + used);
	m_z.avail_out = nsize - used;
}


struct proto_replace_stream::accum_set_comp {
	bool operator() (const shared_stream_accumulator& x, const address& y) const
		{ return x->addr() < y; }
	bool operator() (const address& x, const shared_stream_accumulator& y) const
		{ return x < y->addr(); }
	bool operator() (const shared_stream_accumulator& x, const shared_stream_accumulator& y) const
		{ return x->addr() < y->addr(); }
};


proto_replace_stream::offer_storage::offer_storage(
		const std::string& basename, ClockTime replace_time) :
	m_basename(basename),
	m_replace_time(replace_time) { }

proto_replace_stream::offer_storage::~offer_storage() { }

void proto_replace_stream::offer_storage::add(
		const address& addr,
		const char* key, size_t keylen,
		const char* val, size_t vallen)
{
	accum_set_t::iterator it = accum_set_find(m_set, addr);
	if(it != m_set.end()) {
		(*it)->add(key, keylen, val, vallen);
	} else {
		shared_stream_accumulator accum(new stream_accumulator(m_basename, addr, m_replace_time));
		//m_set.insert(it, accum);  // FIXME
		m_set.push_back(accum);
		std::sort(m_set.begin(), m_set.end(), accum_set_comp());
		accum->add(key, keylen, val, vallen);
	}
}

void proto_replace_stream::offer_storage::commit(accum_set_t* dst)
{
	*dst = m_set;
}


proto_replace_stream::accum_set_t::iterator proto_replace_stream::accum_set_find(
		accum_set_t& accum_set, const address& addr)
{
	accum_set_t::iterator it =
		std::lower_bound(accum_set.begin(), accum_set.end(),
				addr, accum_set_comp());
	if(it != accum_set.end() && (*it)->addr() == addr) {
		return it;
	} else {
		return accum_set.end();
	}
}


int proto_replace_stream::stream_accumulator::openfd(const std::string& basename)
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

proto_replace_stream::stream_accumulator::stream_accumulator(const std::string& basename,
		const address& addr, ClockTime replace_time):
	m_addr(addr),
	m_replace_time(replace_time),
	m_fd(openfd(basename)),
	m_mmap(new mmap_stream(m_fd.get()))
{
	LOG_TRACE("create stream_accumulator for ",addr);
}

proto_replace_stream::stream_accumulator::~stream_accumulator() { }


void proto_replace_stream::stream_accumulator::add(
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

void proto_replace_stream::stream_accumulator::send(int sock)
{
	m_mmap->flush();
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


void proto_replace_stream::stream_accepted(int fd, int err)
try {
	LOG_TRACE("stream accepted fd(",fd,") err:",err);

	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(err));
		net->signal_end();
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

	// take out stream_accumulator from m_accum_set
	shared_stream_accumulator accum;
	{
		pthread_scoped_lock oflk(m_offer_map_mutex);
		accum_set_t::iterator it = accum_set_find(m_accum_set, iaddr);
		if(it == m_accum_set.end()) {
			LOG_DEBUG("storage offer to ",iaddr," is already timed out");
			return;
		}
		accum = *it;
		m_accum_set.erase(it);
	}

	LOG_DEBUG("send offer storage to ",iaddr);
	accum->send(fd);
	LOG_DEBUG("finish to send offer storage to ",iaddr);

	pthread_scoped_lock relk(net->scope_proto_replace().state_mutex());
	net->scope_proto_replace().replace_offer_pop(accum->replace_time(), relk);

} catch (std::exception& e) {
	LOG_WARN("failed to send offer storage: ",e.what());
	throw;
} catch (...) {
	LOG_WARN("failed to send offer storage: unknown error");
	throw;
}


void proto_replace_stream::stream_connected(int fd, int err)
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
		addrbuf[0] = (uint8_t)net->addr().dump_size();
		::memcpy(addrbuf+1, net->addr().dump(), net->addr().dump_size());
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

	m_stream_core->add<stream_handler>(fd);
	fdscope.release();

} catch (std::exception& e) {
	LOG_WARN("failed to receve offer storage: ",e.what());
	throw;
} catch (...) {
	LOG_WARN("failed to receve offer storage: unknown error");
	throw;
}


class proto_replace_stream::stream_handler : public mp::wavy::handler {
public:
	stream_handler(int fd);
	~stream_handler();

	void read_event();
	void submit_message(rpc::msgobj msg, rpc::auto_zone& z);

private:
	msgpack::unpacker m_pac;
	z_stream m_z;
	char* m_buffer;

private:
	stream_handler();
	stream_handler(const stream_handler&);
};

proto_replace_stream::stream_handler::stream_handler(int fd) :
	mp::wavy::handler(fd),
	m_pac(RPC_INITIAL_BUFFER_SIZE)
{
	m_buffer = (char*)::malloc(RPC_INITIAL_BUFFER_SIZE);
	if(!m_buffer) {
		throw std::bad_alloc();
	}

	m_z.zalloc = Z_NULL;
	m_z.zfree = Z_NULL;
	m_z.opaque = Z_NULL;

	if(inflateInit(&m_z) != Z_OK) {
		::free(m_buffer);
		throw std::runtime_error(m_z.msg);
	}
}

proto_replace_stream::stream_handler::~stream_handler()
{
	inflateEnd(&m_z);
	::free(m_buffer);
}

void proto_replace_stream::stream_handler::read_event()
try {
	ssize_t rl = ::read(fd(), m_buffer, RPC_INITIAL_BUFFER_SIZE);
	if(rl < 0) {
		if(errno == EAGAIN || errno == EINTR) {
			return;
		} else {
			throw std::runtime_error("read error");
		}
	} else if(rl == 0) {
		throw std::runtime_error("connection closed");
	}

	m_z.next_in = (Bytef*)m_buffer;
	m_z.avail_in = rl;

	while(true) {
		if(m_pac.buffer_capacity() < RPC_BUFFER_RESERVATION_SIZE) { // FIXME size
			m_pac.reserve_buffer(KUMO_OFFER_INITIAL_MAP_SIZE); // FIXME size
		}

		m_z.next_out = (Bytef*)m_pac.buffer();
		m_z.avail_out = m_pac.buffer_capacity();

		int ret = inflate(&m_z, Z_SYNC_FLUSH);
		if(ret != Z_OK && ret != Z_STREAM_END) {
			throw std::runtime_error("inflate failed");
		}

		m_pac.buffer_consumed( m_pac.buffer_capacity() - m_z.avail_out );

		if(m_z.avail_in == 0) {
			break;
		}
	}

	while(m_pac.execute()) {
		rpc::msgobj msg = m_pac.data();
		std::auto_ptr<msgpack::zone> z( m_pac.release_zone() );
		m_pac.reset();
		submit_message(msg, z);
	}

} catch(msgpack::type_error& e) {
	LOG_ERROR("rpc packet: type error");
	throw;
} catch(std::exception& e) {
	LOG_WARN("rpc packet: ", e.what());
	throw;
} catch(...) {
	LOG_ERROR("rpc packet: unknown error");
	throw;
}


void proto_replace_stream::stream_handler::submit_message(rpc::msgobj msg, rpc::auto_zone& z)
{
	msgpack::type::tuple<msgtype::DBKey, msgtype::DBValue> kv(msg);
	msgtype::DBKey key = kv.get<0>();
	msgtype::DBValue val = kv.get<1>();

	// FIXME updatev
	share->db().update(
			key.raw_data(), key.raw_size(),
			val.raw_data(), val.raw_size());

	// update() returns false means that key is overwritten while replicating.
}


}  // namespace server
}  // namespace kumo

