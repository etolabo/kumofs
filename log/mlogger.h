#ifndef MLOGGER_H__
#define MLOGGER_H__

#include <sstream>
#include <iostream>


#ifndef MLOGGER_LEVEL

#ifdef NDEBUG
#define MLOGGER_LEVEL 2
#else
#define MLOGGER_LEVEL 0
#endif

#endif


class mlogger_initializer;

class mlogger {
public:
	static void reset(mlogger* lg);
	static void destroy();

public:
	static mlogger& instance();

public:
	enum level {
		TRACE  = 0,
		DEBUG  = 1,
		INFO   = 2,
		WARN   = 3,
		ERROR  = 4,
		FATAL  = 5,
	};

	mlogger(level runtime_level);
	virtual ~mlogger();

#define MLOGGER_IMPL_BEGIN \
	try { \
		if(lv < m_runtime_level) { return; } \
		std::stringstream s

#define MLOGGER_IMPL_END \
		std::string str(s.str()); \
		log_impl(lv, str); \
	} catch (...) { \
		std::cerr << prefix << " log error" << std::endl; \
	}


	template <typename A0>
	void log(level lv, const char* prefix, A0 a0) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1>
	void log(level lv, const char* prefix, A0 a0, A1 a1) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6 << a7;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10 << a11;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10 << a11 << a12;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10 << a11 << a12 << a13;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10 << a11 << a12 << a13 << a14;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10 << a11 << a12 << a13 << a14 << a15;
		MLOGGER_IMPL_END;
	}

	template <typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
	void log(level lv, const char* prefix, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15, A16 a16) {
		MLOGGER_IMPL_BEGIN;
		s << prefix << a0 << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10 << a11 << a12 << a13 << a14 << a15 << a16;
		MLOGGER_IMPL_END;
	}


private:
	virtual void log_impl(level lv, std::string& str) = 0;

private:
	level m_runtime_level;

private:
	friend class mlogger_initializer;
	static mlogger* s_logger;

private:
	mlogger();
	mlogger(const mlogger&);
};

inline mlogger& mlogger::instance()
{
	return *s_logger;
}


#include "mlogger_null.h"

static unsigned long mlogger_initializer_counter = 0;

class mlogger_initializer {
public:
	mlogger_initializer()
	{
		if(0 == mlogger_initializer_counter++) {
			if(mlogger::s_logger == NULL) {
				mlogger::reset(new mlogger_null());
			}
		}
	}
	~mlogger_initializer()
	{
		if(0 == --mlogger_initializer_counter) {
			mlogger::destroy();
		}
	}
private:
	void initialize();
};

static mlogger_initializer mlogger_initializer_;

#define MLOGGER_XSTR(s) #s
#define MLOGGER_XSTR_(x) MLOGGER_XSTR(x)
#define MLOGGER_LINE   MLOGGER_XSTR_(__LINE__)

#ifndef MLOGGER_PREFIX
#define MLOGGER_PREFIX __FILE__ ":" MLOGGER_LINE ": "
#endif

#ifndef MLOGGER_PREFIX_VERBOSE
#define MLOGGER_PREFIX_VERBOSE __FILE__ ":" MLOGGER_LINE ":", __FUNCTION__, ": "
#endif



#ifndef MLOGGER_PREFIX_TRACE
#define MLOGGER_PREFIX_TRACE MLOGGER_PREFIX_VERBOSE
#endif

#ifndef MLOGGER_PREFIX_DEBUG
#define MLOGGER_PREFIX_DEBUG MLOGGER_PREFIX_VERBOSE
#endif


#ifndef MLOGGER_PREFIX_INFO
#define MLOGGER_PREFIX_INFO MLOGGER_PREFIX
#endif

#ifndef MLOGGER_PREFIX_WARN
#define MLOGGER_PREFIX_WARN MLOGGER_PREFIX
#endif

#ifndef MLOGGER_PREFIX_ERROR
#define MLOGGER_PREFIX_ERROR MLOGGER_PREFIX
#endif

#ifndef MLOGGER_PREFIX_FATAL
#define MLOGGER_PREFIX_FATAL MLOGGER_PREFIX
#endif



#if MLOGGER_LEVEL <= 0
#define LOG_TRACE(...) \
	mlogger::instance().log(mlogger::TRACE, MLOGGER_PREFIX_TRACE, __VA_ARGS__)
#else
#define LOG_TRACE(...) ((void)0)
#endif

#if MLOGGER_LEVEL <= 1
#define LOG_DEBUG(...) \
	mlogger::instance().log(mlogger::DEBUG, MLOGGER_PREFIX_DEBUG, __VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0)
#endif

#if MLOGGER_LEVEL <= 2
#define LOG_INFO(...) \
	mlogger::instance().log(mlogger::INFO, MLOGGER_PREFIX_INFO, __VA_ARGS__)
#else
#define LOG_INFO(...) ((void)0)
#endif

#if MLOGGER_LEVEL <= 3
#define LOG_WARN(...) \
	mlogger::instance().log(mlogger::WARN, MLOGGER_PREFIX_WARN, __VA_ARGS__)
#else
#define LOG_WARN(...) ((void)0)
#endif

#if MLOGGER_LEVEL <= 4
#define LOG_ERROR(...) \
	mlogger::instance().log(mlogger::ERROR, MLOGGER_PREFIX_ERROR, __VA_ARGS__)
#else
#define LOG_ERROR(...) ((void)0)
#endif

#if MLOGGER_LEVEL <= 5
#define LOG_FATAL(...) \
	mlogger::instance().log(mlogger::FATAL, MLOGGER_PREFIX_FATAL, __VA_ARGS__)
#else
#define LOG_FATAL(...) ((void)0)
#endif


#endif /* mlogger.h */

