#include "rpc/session.h"
#include "rpc/protocol.h"
#include "log/mlogger.h" //FIXME
#include <iterator>

namespace rpc {


basic_session::~basic_session()
{
	if(mp::iothreads::is_end()) { return; }

	msgpack::object res;
	res.type = msgpack::type::NIL;
	msgpack::object err;
	err.type = msgpack::type::POSITIVE_INTEGER;
	err.via.u64 = protocol::TRANSPORT_LOST_ERROR;

	destroy(res, err);
}

session::~session() { }


void basic_session::process_response(
		basic_shared_session& self,
		msgobj result, msgobj error,
		msgid_t msgid, msgpack::zone* new_z)
{
	auto_zone z(new_z);
	callbacks_t::iterator it(m_callbacks.find(msgid));
	if(it == m_callbacks.end()) { return; }
	it->second.callback(self, result, error, z);
	m_callbacks.erase(it);
}


//void basic_session::delegate_to(basic_session& guardian)
//{
//	if(!m_callbacks.empty()) {
//		if(!guardian.m_callbacks.empty()) {
//			guardian.m_callbacks.insert(
//					m_callbacks.begin(), m_callbacks.end());
//			m_callbacks.clear();
//		} else {
//			m_callbacks.swap(guardian.m_callbacks);
//		}
//		guardian.m_msgid_rr = m_msgid_rr;
//	}
//}
//
//void session::delegate_to(session& guardian)
//{
//	if(this == &guardian) { return; }
//	if(!m_pending_queue.empty()) {
//		if(!guardian.m_pending_queue.empty()) {
//			guardian.m_pending_queue.insert(
//					guardian.m_pending_queue.end(),
//					m_pending_queue.begin(), m_pending_queue.end());
//			m_pending_queue.clear();
//		} else {
//			m_pending_queue.swap(guardian.m_pending_queue);
//		}
//	}
//	basic_session::delegate_to(guardian);
//}

void basic_session::send_buffer(const char* buf, size_t buflen,
		void (*finalize)(void*), void* data)
{
	basic_transport::send_data(m_binds[0], buf, buflen,
			finalize, data);
}

void basic_session::send_bufferv(vrefbuffer* buf,
		void (*finalize)(void*), void* data)
{
	basic_transport::send_datav(m_binds[0], buf,
			finalize, data);
}


void basic_session::send_response_data(const char* buf,
		size_t buflen, void (*finalize)(void*), void* data)
{
	mp::pthread_scoped_lock lk(m_binds_mutex);
	if(!is_bound()) {
		throw std::runtime_error("session not bound");
	}
	send_buffer(buf, buflen, finalize, data);
}

void basic_session::send_response_datav(vrefbuffer* buf,
		void (*finalize)(void*), void* data)
{
	mp::pthread_scoped_lock lk(m_binds_mutex);
	if(!is_bound()) {
		throw std::runtime_error("session not bound");
	}
	send_bufferv(buf, finalize, data);
}


bool basic_session::bind_transport(int fd)
{
	mp::pthread_scoped_lock lk(m_binds_mutex);

	m_connect_retried_count = 0;

	bool ret = m_binds.empty() ? true : false;
	m_binds.push_back(fd);

	return ret;
}

bool session::bind_transport(int fd)
{
	mp::pthread_scoped_lock lk(m_binds_mutex);

	m_connect_retried_count = 0;

	bool ret = m_binds.empty() ? true : false;
	m_binds.push_back(fd);

	pending_queue_t pendings;
	pendings.swap(m_pending_queue);

	lk.unlock();

	for(pending_queue_t::iterator it(pendings.begin()), it_end(pendings.end());
			it != it_end; ++it) {
		basic_transport::send_datav(fd, *it, &mp::iothreads::writer::finalize_nothing, (void*)NULL);
	}

	return ret;
}


bool basic_session::unbind_transport(int fd, basic_shared_session& self)
{
	mp::pthread_scoped_lock lk(m_binds_mutex);
	m_binds.erase(std::remove(m_binds.begin(), m_binds.end(), fd), m_binds.end());
	if(m_binds.empty()) {
		lk.unlock();
		if(m_manager) {
			if(mp::iothreads::is_end()) { return true; }
			mp::iothreads::submit(&session_manager::transport_lost_notify, m_manager, self);
		}
		return true;
	}
	return false;
}

bool session::unbind_transport(int fd, basic_shared_session& self)
{
	return basic_session::unbind_transport(fd, self);
}


void basic_session::force_lost(msgobj res, msgobj err)
{
	m_lost = true;
	basic_shared_session nulls;
	for(callbacks_t::iterator it(m_callbacks.begin()), it_end(m_callbacks.end());
			it != it_end; ++it) {
		it->second.callback(nulls, res, err);
	}
	m_callbacks.clear();
}

void basic_session::destroy(msgobj res, msgobj err)
{
	//mp::pthread_scoped_lock lk(m_binds_mutex);
	m_lost = true;
	basic_shared_session nulls;
	for(callbacks_t::iterator it(m_callbacks.begin()), it_end(m_callbacks.end());
			it != it_end; ++it) {
		it->second.callback_submit(nulls, res, err);
	}
	//m_callbacks.clear();
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
	m_life->push_finalizer(&mp::object_delete<msgpack::zone>, z.get());
	z.release();
	callback(s, res, err);
}

void basic_session::callback_entry::callback(basic_shared_session& s,
		msgobj res, msgobj err)
try {
	m_callback(s, res, err, m_life);
} catch (std::exception& e) {
	LOG_ERROR("response callback error: ",e.what());
} catch (...) {
	LOG_ERROR("response callback error: unknown error");
}

void basic_session::callback_entry::callback_submit(basic_shared_session& s,
		msgobj res, msgobj err)
{
	mp::iothreads::submit(m_callback, s, res, err, m_life);
}

void basic_session::step_timeout(basic_shared_session self)
{
	msgpack::object res;
	res.type = msgpack::type::NIL;
	msgpack::object err;
	err.type = msgpack::type::POSITIVE_INTEGER;
	err.via.u64 = protocol::TIMEOUT_ERROR;

	for(callbacks_t::iterator it(m_callbacks.begin()), it_end(m_callbacks.end());
			it != it_end; ) {
		if(!it->second.step_timeout()) {
			it->second.callback(self, res, err);
			m_callbacks.erase(it++);
		} else {
			++it;
		}
	}
}

bool basic_session::callback_entry::step_timeout()
{
	if(m_timeout_steps > 0) {
		--m_timeout_steps;
		return true;
	}
	return false;
}


}  // namespace rpc

