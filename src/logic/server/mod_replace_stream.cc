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
#include "server/framework.h"
#include "server/mod_replace_stream.h"
#include "server/zmmap_stream.h"
#include "server/zconnection.h"
#include <mp/exception.h>
#include <mp/utility.h>
#include <fcntl.h>
#include <algorithm>

#if defined(__linux__) || defined(__sun__)
#include <sys/sendfile.h>
#endif

namespace kumo {
namespace server {


mod_replace_stream_t::mod_replace_stream_t(address stream_addr) :
	m_stream_addr(stream_addr),
	send_offer_counter(0)
{ }

mod_replace_stream_t::~mod_replace_stream_t() { }

void mod_replace_stream_t::init_stream(int fd)
{
	m_stream_core.reset(new mp::wavy::core());
	using namespace mp::placeholders;
	m_stream_core->listen(fd, mp::bind(
				&mod_replace_stream_t::stream_accepted, this,
				_1, _2));
	m_stream_core->add_thread(2);  // FIXME 2
}

void mod_replace_stream_t::stop_stream()
{
	m_stream_core->end();
}

class mod_replace_stream_t::stream_accumulator {
public:
	stream_accumulator(const std::string& basename,
			const address& addr, ClockTime replace_time);
	~stream_accumulator();
public:
	void add(const char* key, size_t keylen,
			const char* val, size_t vallen);
	void flush();
	void send(int sock);

	const address& addr() const { return m_addr; }
	ClockTime replace_time() const { return m_replace_time; }

	uint64_t num_itmes() const { return m_items; }
	size_t stream_size() const { return m_mmap_stream->size(); }

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

	std::auto_ptr<zmmap_stream> m_mmap_stream;

	uint64_t m_items;

private:
	stream_accumulator();
	stream_accumulator(const stream_accumulator&);
};


RPC_IMPL(mod_replace_stream_t, ReplaceOffer, req, z, response)
{
	const address& stream_addr( req.param().addr );
	char addrbuf[stream_addr.addrlen()];
	stream_addr.getaddr((sockaddr*)addrbuf);

	using namespace mp::placeholders;
	m_stream_core->connect(
			PF_INET, SOCK_STREAM, 0,
			(sockaddr*)addrbuf, sizeof(addrbuf),
			net->connect_timeout_msec(),
			mp::bind(&mod_replace_stream_t::stream_connected, this, _1, _2));

	// Note: response: don't return any result
	LOG_INFO("send replace offer to ",stream_addr);
}


void mod_replace_stream_t::send_offer(mod_replace_stream_t::offer_storage& offer, ClockTime replace_time)
{
	offer.flush();  // add nil-terminate

	pthread_scoped_lock oflk(m_accum_set_mutex);
	offer.commit(&m_accum_set);

	send_offer_counter++;

	pthread_scoped_lock relk(net->mod_replace.state_mutex());

	for(accum_set_t::iterator it(m_accum_set.begin()),
			it_end(m_accum_set.end()); it != it_end; ++it) {
		const address& addr( (*it)->addr() );

		LOG_DEBUG("send offer to ",(*it)->addr());
		shared_zone nullz;
		mod_replace_stream_t::ReplaceOffer param(m_stream_addr);

		using namespace mp::placeholders;
		net->get_node(addr)->call(param, nullz,
				BIND_RESPONSE(mod_replace_stream_t, ReplaceOffer, addr, send_offer_counter), 160);  // FIXME 160

		net->mod_replace.replace_offer_push(replace_time, relk);
	}
}


RPC_REPLY_IMPL(mod_replace_stream_t, ReplaceOffer, from, res, err, z,
		address addr, uint32_t counter)
{
	LOG_TRACE("ResReplaceOffer from ",addr," res:",res," err:",err);
	// Note: this request always timed out

	if (counter != send_offer_counter) {
		return;
	}

	pthread_scoped_lock oflk(m_accum_set_mutex);

	accum_set_t::iterator it = accum_set_find(m_accum_set, addr);
	if(it == m_accum_set.end()) {
		return;
	}

	m_accum_set.erase(it);
}



struct mod_replace_stream_t::accum_set_comp {
	bool operator() (const shared_stream_accumulator& x, const address& y) const
		{ return x->addr() < y; }
	bool operator() (const address& x, const shared_stream_accumulator& y) const
		{ return x < y->addr(); }
	bool operator() (const shared_stream_accumulator& x, const shared_stream_accumulator& y) const
		{ return x->addr() < y->addr(); }
};


mod_replace_stream_t::offer_storage::offer_storage(
		const std::string& basename, ClockTime replace_time) :
	m_basename(basename),
	m_replace_time(replace_time) { }

mod_replace_stream_t::offer_storage::~offer_storage() { }

void mod_replace_stream_t::offer_storage::add(
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

size_t mod_replace_stream_t::offer_storage::stream_size(const address& addr)
{
	accum_set_t::iterator it = accum_set_find(m_set, addr);
	if(it != m_set.end()) {
		return (*it)->stream_size();
	} else {
		return 0;
	}
}

void mod_replace_stream_t::offer_storage::flush()
{
	for(accum_set_t::iterator it(m_set.begin()),
			it_end(m_set.end()); it != it_end; ++it) {
		(*it)->flush();
	}
}

void mod_replace_stream_t::offer_storage::commit(accum_set_t* dst)
{
	*dst = m_set;
}


mod_replace_stream_t::accum_set_t::iterator mod_replace_stream_t::accum_set_find(
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


int mod_replace_stream_t::stream_accumulator::openfd(const std::string& basename)
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

mod_replace_stream_t::stream_accumulator::stream_accumulator(const std::string& basename,
		const address& addr, ClockTime replace_time) :
	m_addr(addr),
	m_replace_time(replace_time),
	m_fd(openfd(basename)),
	m_mmap_stream(new zmmap_stream(m_fd.get())),
	m_items(0)
{
	LOG_TRACE("create stream_accumulator for ",addr);
}

mod_replace_stream_t::stream_accumulator::~stream_accumulator() { }


void mod_replace_stream_t::stream_accumulator::add(
		const char* key, size_t keylen,
		const char* val, size_t vallen)
{
	msgpack::packer<zmmap_stream> pk(*m_mmap_stream);
	pk.pack_array(2);
	pk.pack_raw(keylen);
	pk.pack_raw_body(key, keylen);
	pk.pack_raw(vallen);
	pk.pack_raw_body(val, vallen);
	++m_items;
}

void mod_replace_stream_t::stream_accumulator::flush()
{
	msgpack::packer<zmmap_stream> pk(*m_mmap_stream);
	pk.pack_nil();
}

void mod_replace_stream_t::stream_accumulator::send(int sock)
{
	m_mmap_stream->flush();
	size_t size = m_mmap_stream->size();
	//m_mmap_stream.reset(NULL);  // FIXME needed?
#if defined(__linux__) || defined(__sun__)
	while(size > 0) {
		ssize_t rl = ::sendfile(sock, m_fd.get(), NULL, size);
		if(rl <= 0) { throw mp::system_error(errno, "offer send error"); }
		size -= rl;
	}
#elif defined(__APPLE__) && defined(__MACH__)
	// Mac OS X
	off_t sent = 0;
	while(sent < size) {
		off_t len = size - sent;
		if(::sendfile(m_fd.get(), sock, sent, &len, NULL, 0) < 0) {
			throw mp::system_error(errno, "offer send error");
		}
		sent += len;
	}
#else
	size_t sent = 0;
	while(sent < size) {
		size_t len = size - sent;
		off_t sbytes = 0;
		if(::sendfile(m_fd.get(), sock, sent, len, NULL, &sbytes, 0) < 0) {
			throw mp::system_error(errno, "offer send error");
		}
		sent += sbytes;
	}
#endif
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


void mod_replace_stream_t::stream_accepted(int fd, int err)
try {
	LOG_TRACE("stream accepted fd(",fd,") err:",err);

	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(err));
		net->signal_end();
		return;
	}

	scopeout_close fdscope(fd);
	if(::fcntl(fd, F_SETFL, 0) < 0) {  // set blocking mode
		LOG_ERROR("stream connect: fcntl failed: ", strerror(err));
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
				LOG_ERROR("failed to recv init address: ", strerror(err));
				return;
			}
			if((size_t)rl >= sz) { break; }
			sz -= rl;
			p += rl;
		}
		iaddr = address(addrbuf+1, (uint8_t)addrbuf[0]);
	}

	// take stream_accumulator from m_accum_set
	shared_stream_accumulator accum;
	{
		pthread_scoped_lock oflk(m_accum_set_mutex);
		accum_set_t::iterator it = accum_set_find(m_accum_set, iaddr);
		if(it == m_accum_set.end()) {
			LOG_ERROR("storage offer to ",iaddr," is already timed out");
			return;
		}
		accum = *it;

		LOG_DEBUG("send offer storage to ",iaddr);

		accum->send(fd);

		struct timeval timeout = {60, 0};  // FIXME
		if(::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
					&timeout, sizeof(timeout)) < 0) {
			throw std::runtime_error("can't set SO_RCVTIMEO");
		}

		msgpack::unpacker pac;

		while(true) {
			pac.reserve_buffer(1024);
			ssize_t rl = ::read(fd, pac.buffer(), pac.buffer_capacity());
			if(rl <= 0) {
				if(errno == EINTR) { continue; }
				if(errno == EAGAIN) {
					throw std::runtime_error("read stream response timed out");
				} else {
					throw std::runtime_error("can't read stream response");
				}
			}

			pac.buffer_consumed(rl);

			bool done = false;
			while(pac.execute()) {
				msgpack::object msg = pac.data();
				std::auto_ptr<msgpack::zone> z( pac.release_zone() );
				pac.reset();
				if(msg.is_nil()) {
					done = true;
					break;
				}
			}
			if(done) { break; }
		}

		m_accum_set.erase(it);
	}

	LOG_DEBUG("finish to send offer storage to ",iaddr);

	pthread_scoped_lock relk(net->mod_replace.state_mutex());
	net->mod_replace.replace_offer_pop(accum->replace_time(), relk);

} catch (std::exception& e) {
	LOG_WARN("failed to send offer storage: ",e.what());
	throw;
} catch (...) {
	LOG_WARN("failed to send offer storage: unknown error");
	throw;
}


void mod_replace_stream_t::stream_connected(int fd, int err)
try {
	LOG_TRACE("stream connected fd(",fd,") err: ",err);
	if(fd < 0) {
		LOG_ERROR("stream connect failed: ", strerror(err));
		return;
	}

	scopeout_close fdscope(fd);

	if(::fcntl(fd, F_SETFL, 0) < 0) {  // set blocking mode
		LOG_ERROR("stream connect: fcntl failed: ", strerror(err));
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
				LOG_ERROR("failed to send init address: ", strerror(err));
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


class mod_replace_stream_t::stream_handler : public zconnection<stream_handler> {
public:
	stream_handler(int fd) :
		zconnection<stream_handler>(fd),
		m_items(0), m_major_counter(0), m_minor_counter(0) { }

	~stream_handler() { }

	void submit_message(rpc::msgobj msg, rpc::auto_zone& z);

private:
	uint64_t m_items;
	uint64_t m_major_counter;
	volatile uint64_t m_minor_counter;
};

void mod_replace_stream_t::stream_handler::submit_message(rpc::msgobj msg, rpc::auto_zone& z)
{
	if(msg.is_nil()) {
		msgpack::sbuffer tmpbuf(32);
		msgpack::packer<msgpack::sbuffer>(tmpbuf).pack_nil();
		wavy::write(fd(), tmpbuf.data(), tmpbuf.size());
		return;
	}

	msgpack::type::tuple<msgtype::DBKey, msgtype::DBValue> kv(msg);
	msgtype::DBKey key = kv.get<0>();
	msgtype::DBValue val = kv.get<1>();

	// FIXME updatev
	share->db().update(
			key.raw_data(), key.raw_size(),
			val.raw_data(), val.raw_size());

	// update() returns false means that key is overwritten while replicating.

	if((++m_major_counter) % 100 == 0) {
		m_minor_counter += 1;

		// send keepalive
		msgpack::sbuffer tmpbuf(32);
		msgpack::packer<msgpack::sbuffer> pk(tmpbuf);
		pk.pack_array(0);
		wavy::write(fd(), tmpbuf.data(), tmpbuf.size());
	}
}


}  // namespace server
}  // namespace kumo

