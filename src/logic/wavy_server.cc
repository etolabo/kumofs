#include "logic/wavy_server.h"
#include <mp/object_callback.h>

namespace kumo {


wavy_server::wavy_server() :
	m_core_threads(0), m_output_threads(0)
{ }

wavy_server::~wavy_server() { }


void wavy_server::do_after(unsigned int steps, mp::function<void ()> func)
{
	mp::pthread_scoped_lock dalk(m_do_after_mutex);
	m_do_after.push_back( do_after_entry(steps, func) );
}

void wavy_server::step_do_after()
{
	do_after_t fire;

	{
		mp::pthread_scoped_lock dalk(m_do_after_mutex);
		for(do_after_t::iterator it(m_do_after.begin()); it != m_do_after.end(); ) {
			if(it->remain_steps == 0) {
				fire.splice(fire.end(), m_do_after, it++);
			} else {
				--it->remain_steps;
				++it;
			}
		}
	}

	for(do_after_t::iterator it(fire.begin()); it != fire.end(); ++it) {
		try {
			// FIXME wavy::submit?
			it->func();
		} catch (...) { }  // FIXME log
	}
}


namespace {
	// avoid compile error
	typedef void (*sigend_callback)(void*, int);
	static sigend_callback get_signal_handler()
	{
		sigend_callback f = &mp::object_callback<void (int)>::
			mem_fun<wavy_server, &wavy_server::signal_handler>;
		return f;
	}
}  // noname namespace

void wavy_server::init_wavy(unsigned short rthreads, unsigned short wthreads)
{
	// ignore SIGPIPE
	if( signal(SIGPIPE, SIG_IGN) == SIG_ERR ) {
		perror("signal");
		throw mp::system_error(errno, "signal");
	}

	// initialize signal handler before starting threads
	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss, SIGHUP);
	sigaddset(&ss, SIGINT);
	sigaddset(&ss, SIGTERM);

	s_pth.reset( new mp::pthread_signal(ss,
				get_signal_handler(),
				reinterpret_cast<void*>(this)) );

	// initialize wavy
	m_core_threads = rthreads;
	m_output_threads = wthreads;
	wavy::initialize(0,0);
}


void wavy_server::run()
{
	wavy::add_output_thread(m_output_threads);
	wavy::add_core_thread(m_core_threads);
}

void wavy_server::join()
{
	wavy::join();
}


void wavy_server::signal_handler(int signo)
{
	if(signo == SIGINT || signo == SIGTERM) {
		signal_end();
	} else {
		signal_hup();
	}
}


// dummy function
static void finished() { }

void wavy_server::signal_end()
{
	wavy::end();
	wavy::submit(finished);  // submit dummy function
	end_preprocess();
	LOG_INFO("end");
}

void wavy_server::signal_hup()
{
	LOG_INFO("SIGHUP");
	if(logpacker::is_active()) {
		logpacker::reopen();
	}
}


}  // namespace kumo

