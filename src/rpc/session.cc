#include "rpc/rpc.h"
#include "rpc/protocol.h"
#include "log/mlogger.h" //FIXME
#include <iterator>

namespace rpc {


namespace {

class callback_entry {
public:
	callback_entry();
	callback_entry(callback_t callback, shared_zone life,
			unsigned short timeout_steps);

public:
	void callback(basic_shared_session& s, msgobj res, msgobj err, auto_zone& z);
	void callback(basic_shared_session& s, msgobj res, msgobj err);
	inline void callback_submit(basic_shared_session& s, msgobj res, msgobj err);
	inline bool step_timeout();  // Note: NOT thread-safe

private:
	void callback_real(basic_shared_session& s,
			msgobj res, msgobj err, shared_zone z);

private:
	unsigned short m_timeout_steps;
	callback_t m_callback;
	shared_zone m_life;
};

class callback_table {
public:
	callback_table();
	~callback_table();

public:
	void insert(msgid_t msgid, const callback_entry& entry);
	bool out(msgid_t msgid, callback_entry* result);
	template <typename F> void for_each_clear(F f);
	template <typename F> void erase_if(F f);

private:
	static const size_t PARTITION_NUM = 4;  // FIXME
	typedef std::map<msgid_t, callback_entry> callbacks_t;
	mp::pthread_mutex m_callbacks_mutex[PARTITION_NUM];
	callbacks_t m_callbacks[PARTITION_NUM];

private:
	callback_table(const callback_table&);
};


callback_entry::callback_entry() { }

callback_entry::callback_entry(
		callback_t callback, shared_zone life,
		unsigned short timeout_steps) :
	m_timeout_steps(timeout_steps),
	m_callback(callback),
	m_life(life) { }

void callback_entry::callback(basic_shared_session& s,
		msgobj res, msgobj err, auto_zone& z)
{
	// msgpack::zone::push_finalizer is not thread-safe
	// m_life may null. see {basic_,}session::call
	//m_life->push_finalizer(&mp::object_delete<msgpack::zone>, z.release());
	shared_zone life(z.release());
	if(m_life) { life->allocate<shared_zone>(m_life); }
	callback_real(s, res, err, life);
}

void callback_entry::callback(basic_shared_session& s,
		msgobj res, msgobj err)
{
	shared_zone life = m_life;
	if(!life) { life.reset(new msgpack::zone()); }
	callback_real(s, res, err, life);
}

void callback_entry::callback_submit(
		basic_shared_session& s, msgobj res, msgobj err)
{
	shared_zone life = m_life;
	if(!life) { life.reset(new msgpack::zone()); }
	wavy::submit(m_callback, s, res, err, life);
}

inline void callback_entry::callback_real(basic_shared_session& s,
		msgobj res, msgobj err, shared_zone life)
try {
	m_callback(s, res, err, life);
} catch (std::exception& e) {
	LOG_ERROR("response callback error: ",e.what());
} catch (...) {
	LOG_ERROR("response callback error: unknown error");
}

bool callback_entry::step_timeout()
{
	if(m_timeout_steps > 0) {
		--m_timeout_steps;  // FIXME atomic?
		return true;
	}
	return false;
}


callback_table::callback_table() { }

callback_table::~callback_table() { }

bool callback_table::out(
		msgid_t msgid, callback_entry* result)
{
	pthread_scoped_lock lk(m_callbacks_mutex[msgid % PARTITION_NUM]);

	callbacks_t& cbs(m_callbacks[msgid % PARTITION_NUM]);
	callbacks_t::iterator it(cbs.find(msgid));
	if(it == cbs.end()) {
		return false;
	}

	*result = it->second;
	cbs.erase(it);

	return true;
}

template <typename F>
void callback_table::for_each_clear(F f)
{
	for(size_t i=0; i < PARTITION_NUM; ++i) {
		pthread_scoped_lock lk(m_callbacks_mutex[i]);
		callbacks_t& cbs(m_callbacks[i]);
		std::for_each(cbs.begin(), cbs.end(), f);
		cbs.clear();
	}
}

template <typename F>
void callback_table::erase_if(F f)
{
	for(size_t i=0; i < PARTITION_NUM; ++i) {
		pthread_scoped_lock lk(m_callbacks_mutex[i]);
		callbacks_t& cbs(m_callbacks[i]);
		for(callbacks_t::iterator it(cbs.begin()), it_end(cbs.end());
				it != it_end; ) {
			if(f(*it)) {
				cbs.erase(it++);
			} else {
				++it;
			}
		}
		//cbs.erase(std::remove_if(cbs.begin(), cbs.end(), f), cbs.end());
	}
}

void callback_table::insert(
		msgid_t msgid, const callback_entry& entry)
{
	pthread_scoped_lock lk(m_callbacks_mutex[msgid % PARTITION_NUM]);
	std::pair<callbacks_t::iterator, bool> pair =
		m_callbacks[msgid % PARTITION_NUM].insert(
				callbacks_t::value_type(msgid, entry));
	if(!pair.second) {
		pair.first->second = entry;
	}
}

}  // noname namespace


#define ANON_m_cbtable \
	reinterpret_cast<callback_table*>(m_cbtable)


basic_session::basic_session(session_manager* mgr) :
	m_msgid_rr(0),  // FIXME randomize?
	m_lost(false),
	m_connect_retried_count(0),
	m_manager(mgr)
{
	m_cbtable = reinterpret_cast<void*>(new callback_table());
}


basic_session::~basic_session()
{
	// FIXME
	//if(mp::iothreads::is_end()) { return; }

	msgpack::object res;
	res.type = msgpack::type::NIL;
	msgpack::object err;
	err.type = msgpack::type::POSITIVE_INTEGER;
	err.via.u64 = protocol::TRANSPORT_LOST_ERROR;

	basic_shared_session nulls;  // FIXME nulls
	force_lost(nulls, res, err);

	delete ANON_m_cbtable;
}


void basic_session::call_real(msgid_t msgid, std::auto_ptr<vrefbuffer> buffer,
		shared_zone life, callback_t callback, unsigned short timeout_steps)
{
	//if(!life) { life.reset(new msgpack::zone()); }

	ANON_m_cbtable->insert(msgid, callback_entry(callback, life, timeout_steps));

	if(is_lost()) {
		//throw std::runtime_error("lost session");
		// FIXME XXX forget the error for robustness and wait timeout.
		return;
	}

	pthread_scoped_lock blk(m_binds_mutex);
	if(m_binds.empty()) {
		//throw std::runtime_error("session not bound");
		// FIXME XXX forget the error for robustness and wait timeout.

	} else {
		// XXX ad-hoc load balancing
		m_binds[m_msgid_rr % m_binds.size()]->send_datav(buffer.get(),
				&mp::object_delete<vrefbuffer>, buffer.get());
		buffer.release();
	}
}

void session::call_real(msgid_t msgid, std::auto_ptr<vrefbuffer> buffer,
		shared_zone life, callback_t callback, unsigned short timeout_steps)
{
	//if(!life) { life.reset(new msgpack::zone()); }

	ANON_m_cbtable->insert(msgid, callback_entry(callback, life, timeout_steps));

	if(is_lost()) {
		//throw std::runtime_error("lost session");
		// FIXME XXX forget the error for robustness and wait timeout.
		return;
	}

	pthread_scoped_lock blk(m_binds_mutex);
	if(m_binds.empty()) {
		{
			pthread_scoped_lock plk(m_pending_queue_mutex);
			LOG_TRACE("push pending queue ",m_pending_queue.size()+1);
			m_pending_queue.push_back(buffer.get());
		}
		buffer.release();
		// FIXME clear pending queue if it is too big
		// FIXME or throw exception

	} else {
		// ad-hoc load balancing
		m_binds[m_msgid_rr % m_binds.size()]->send_datav(buffer.get(),
				&mp::object_delete<vrefbuffer>, buffer.get());
		buffer.release();
	}
}


void basic_session::process_response(
		basic_shared_session& self,
		msgobj result, msgobj error,
		msgid_t msgid, auto_zone z)
{
	callback_entry e;
	LOG_DEBUG("process callback this=",(void*)this," id=",msgid," result:",result," error:",error);
	if(!ANON_m_cbtable->out(msgid, &e)) {
		LOG_DEBUG("callback not found id=",msgid);
		return;
	}
	e.callback(self, result, error, z);
}


void basic_session::send_data(const char* buf, size_t buflen,
		void (*finalize)(void*), void* data)
{
	pthread_scoped_lock lk(m_binds_mutex);
	if(m_binds.empty()) {
		throw std::runtime_error("session not bound");
	}
	// ad-hoc load balancing
	m_binds[m_msgid_rr % m_binds.size()]->send_data(
			buf, buflen, finalize, data);
}

void basic_session::send_datav(vrefbuffer* buf,
		void (*finalize)(void*), void* data)
{
	pthread_scoped_lock lk(m_binds_mutex);
	if(m_binds.empty()) {
		throw std::runtime_error("session not bound");
	}
	// ad-hoc load balancing
	m_binds[m_msgid_rr % m_binds.size()]->send_datav(
			buf, finalize, data);
}


bool basic_session::bind_transport(basic_transport* t)
{
	m_connect_retried_count = 0;

	pthread_scoped_lock lk(m_binds_mutex);

	bool ret = m_binds.empty() ? true : false;
	m_binds.push_back(t);

	return ret;
}

bool session::bind_transport(basic_transport* t)
{
	bool ret = basic_session::bind_transport(t);

	pending_queue_t pendings;
	{
		pthread_scoped_lock lk(m_pending_queue_mutex);
		pendings.swap(m_pending_queue);
	}

	for(pending_queue_t::iterator it(pendings.begin()),
			it_end(pendings.end()); it != it_end; ++it) {
		t->send_datav(*it,
			&mp::object_delete<vrefbuffer>, *it);
	}
	pendings.clear();

	return ret;
}


bool basic_session::unbind_transport(basic_transport* t, basic_shared_session& self)
{
	pthread_scoped_lock lk(m_binds_mutex);

	binds_t::iterator remove_from =
		std::remove(m_binds.begin(), m_binds.end(), t);
	m_binds.erase(remove_from, m_binds.end());

	if(m_binds.empty()) {
		if(m_manager) {
			wavy::submit(&session_manager::transport_lost_notify, m_manager, self);
		}
		return true;
	}
	return false;
}

bool session::unbind_transport(basic_transport* t, basic_shared_session& self)
{
	return basic_session::unbind_transport(t, self);
}


void basic_session::shutdown()
{
	pthread_scoped_lock lk(m_binds_mutex);

	basic_shared_session self;
	for(binds_t::iterator it(m_binds.begin()), it_end(m_binds.end());
			it != it_end; ++it) {
		basic_shared_session b = (*it)->shutdown();
		if(b) { self = b; }
	}
	m_binds.clear();

	if(m_manager && self) {
		wavy::submit(&session_manager::transport_lost_notify, m_manager, self);
	}
}


namespace {
	struct each_callback_submit {
		each_callback_submit(basic_shared_session& s,
				msgobj r, msgobj e) :
			self(s), res(r), err(e) { }
		template <typename T>
		void operator() (T& pair) const
		{
			pair.second.callback_submit(self, res, err);
		}
	private:
		basic_shared_session& self;
		msgobj res;
		msgobj err;
		each_callback_submit();
	};
}

void basic_session::force_lost(basic_shared_session& s,
		msgobj res, msgobj err)
{
	m_lost = true;
	ANON_m_cbtable->for_each_clear(each_callback_submit(s, res, err));
}

namespace {
	struct remove_if_step_timeout {
		remove_if_step_timeout(basic_shared_session s) :
			self(s)
		{
			res.type = msgpack::type::NIL;
			err.type = msgpack::type::POSITIVE_INTEGER;
			err.via.u64 = protocol::TIMEOUT_ERROR;
		}
		template <typename T>
		bool operator() (T& pair)
		{
			if(!pair.second.step_timeout()) {
				LOG_DEBUG("callback timeout id=",pair.first);
				pair.second.callback_submit(self, res, err);  // client::step_timeout;
				//pair.second.callback(self, res, err);  // client::step_timeout;  // FIXME XXX
				return true;
			}
			return false;
		}
	private:
		basic_shared_session& self;
		msgobj res;
		msgobj err;
		remove_if_step_timeout();
	};
}  // noname namespace

void basic_session::step_timeout(basic_shared_session self)
{
	ANON_m_cbtable->erase_if(remove_if_step_timeout(self));
}


void session::cancel_pendings()
{
	pthread_scoped_lock lk(m_pending_queue_mutex);
	clear_pending_queue(m_pending_queue);
}

void session::clear_pending_queue(pending_queue_t& queue)
{
	for(pending_queue_t::iterator it(queue.begin()),
			it_end(queue.end()); it != it_end; ++it) {
		delete *it;
	}
	queue.clear();
}


}  // namespace rpc

