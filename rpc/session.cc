#include "rpc/rpc.h"
#include "rpc/protocol.h"
#include "log/mlogger.h" //FIXME
#include <iterator>

namespace rpc {


inline bool basic_session::callback_table::out(
		msgid_t msgid, callback_entry* result)
{
	pthread_scoped_lock lk(m_callbacks_mutex[msgid % PARTITION_NUM]);
	callbacks_t& cbs(m_callbacks[msgid % PARTITION_NUM]);
	callbacks_t::iterator it(cbs.find(msgid));
	if(it == cbs.end()) { return false; }
	*result = it->second;
	cbs.erase(it);
	return true;
}

template <typename F>
inline void basic_session::callback_table::for_each_clear(F f)
{
	for(size_t i=0; i < PARTITION_NUM; ++i) {
		pthread_scoped_lock lk(m_callbacks_mutex[i]);
		callbacks_t& cbs(m_callbacks[i]);
		std::for_each(cbs.begin(), cbs.end(), f);
		cbs.clear();
	}
}

template <typename F>
inline void basic_session::callback_table::erase_if(F f)
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


basic_session::~basic_session()
{
	// FIXME
	//if(mp::iothreads::is_end()) { return; }

	msgpack::object res;
	res.type = msgpack::type::NIL;
	msgpack::object err;
	err.type = msgpack::type::POSITIVE_INTEGER;
	err.via.u64 = protocol::TRANSPORT_LOST_ERROR;

	force_lost(res, err);
}

session::~session()
{
	cancel_pendings();
}


void basic_session::process_response(
		basic_shared_session& self,
		msgobj result, msgobj error,
		msgid_t msgid, auto_zone z)
{
	callback_entry e;
	LOG_DEBUG("process callback this=",(void*)this," id=",msgid," result:",result," error:",error);
	if(!m_cbtable.out(msgid, &e)) {
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
		each_callback_submit(msgobj r, msgobj e) :
			res(r), err(e) { }
		template <typename T>
		void operator() (T& pair) const
		{
			basic_shared_session nulls;
			pair.second.callback_submit(nulls, res, err);
		}
	private:
		msgobj res;
		msgobj err;
		each_callback_submit();
	};
}

void basic_session::force_lost(msgobj res, msgobj err)
{
	m_lost = true;
	m_cbtable.for_each_clear(each_callback_submit(res, err));
}


basic_session::callback_entry::callback_entry() { }

basic_session::callback_entry::callback_entry(
		callback_t callback, shared_zone life,
		unsigned short timeout_steps) :
	m_timeout_steps(timeout_steps),
	m_callback(callback),
	m_life(life) { }

void basic_session::callback_entry::callback(basic_shared_session& s,
		msgobj res, msgobj err, auto_zone& z)
{
	// msgpack::zone::push_finalizer is not thread-safe
	// m_life may null. see {basic_,}session::call
	//m_life->push_finalizer(&mp::object_delete<msgpack::zone>, z.release());
	shared_zone life(z.release());
	if(m_life) { life->allocate<shared_zone>(m_life); }
	callback_real(s, res, err, life);
}

void basic_session::callback_entry::callback(basic_shared_session& s,
		msgobj res, msgobj err)
{
	shared_zone life = m_life;
	if(!life) { life.reset(new msgpack::zone()); }
	callback_real(s, res, err, life);
}

void basic_session::callback_entry::callback_submit(
		basic_shared_session& s, msgobj res, msgobj err)
{
	shared_zone life = m_life;
	if(!life) { life.reset(new msgpack::zone()); }
	wavy::submit(m_callback, s, res, err, life);
}

void basic_session::callback_entry::callback_real(basic_shared_session& s,
		msgobj res, msgobj err, shared_zone life)
try {
	m_callback(s, res, err, life);
} catch (std::exception& e) {
	LOG_ERROR("response callback error: ",e.what());
} catch (...) {
	LOG_ERROR("response callback error: unknown error");
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
}

void basic_session::step_timeout(basic_shared_session self)
{
	m_cbtable.erase_if(remove_if_step_timeout(self));
}

bool basic_session::callback_entry::step_timeout()
{
	if(m_timeout_steps > 0) {
		--m_timeout_steps;  // FIXME atomic?
		return true;
	}
	return false;
}


}  // namespace rpc

