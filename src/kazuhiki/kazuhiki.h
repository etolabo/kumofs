/*
 * Kazuhiki 3
 *
 * Copyright (C) 2007-2008 FURUHASHI Sadayuki
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef KAZUHIKI_H__
#define KAZUHIKI_H__

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>

#ifndef MP_NO_CXX_ABI_H
#include <typeinfo>
#include <cxxabi.h>
#endif

namespace kazuhiki {

void init();

typedef unsigned int (*parse_t)(void* data, int argc, const char** argv);

struct acceptable {
	acceptable();
	acceptable(parse_t, void*, bool);
	parse_t parse;
	void* data;
	bool required;
};

void on(const char* short_name, const char* long_name, acceptable ac);
void on(const char* short_name, const char* long_name, bool* optional, acceptable ac);

void parse(int argc, char** argv);
void break_parse(int& argc, char** argv);
void order(int argc, char** argv);
void break_order(int& argc, char** argv);


struct argument_error : public std::runtime_error {
	argument_error(const std::string& msg) :
		std::runtime_error(msg) { }
};

struct invalid_argument : public argument_error {
	invalid_argument(const std::string& msg) :
		argument_error(msg) { }
};

struct unknown_argument : public argument_error {
	unknown_argument(const std::string& msg) :
		argument_error(msg) { }
};


namespace type {

acceptable boolean(bool* dst);

acceptable string(std::string* dst);
acceptable string(std::string* dst, const std::string& d);

template <typename T> acceptable numeric(T* dst);
template <typename T> acceptable numeric(T* dst, T d);

template <typename F> acceptable action(F f, bool required);

acceptable connectable(sockaddr_in*      dst);
acceptable connectable(sockaddr_in6*     dst);
acceptable connectable(sockaddr_storage* dst);

acceptable connectable(sockaddr_in*      dst, unsigned short d_port);
acceptable connectable(sockaddr_in6*     dst, unsigned short d_port);
acceptable connectable(sockaddr_storage* dst, unsigned short d_port);

acceptable connectable(sockaddr_in*      dst, unsigned short d_port, const char* d_host);
acceptable connectable(sockaddr_in6*     dst, unsigned short d_port, const char* d_host);
acceptable connectable(sockaddr_storage* dst, unsigned short d_port, const char* d_host);
acceptable connectable(sockaddr_storage* dst, const char* d_path);

acceptable listenable(sockaddr_in*      dst);
acceptable listenable(sockaddr_in6*     dst);
acceptable listenable(sockaddr_storage* dst);

acceptable listenable(sockaddr_in*      dst, unsigned short d_port);
acceptable listenable(sockaddr_in6*     dst, unsigned short d_port);
acceptable listenable(sockaddr_storage* dst, unsigned short d_port);

acceptable listenable(sockaddr_in*      dst, unsigned short d_port, const char* d_host);
acceptable listenable(sockaddr_in6*     dst, unsigned short d_port, const char* d_host);
acceptable listenable(sockaddr_storage* dst, const char* d_path);

}  // namespace type


class parser {
public:
	template <typename T>
	T* alloc() { return alloc_real(new T()); }

	template <typename T, typename A1>
	T* alloc(A1 a1) { return alloc_real(new T(a1)); }

	template <typename T, typename A1, typename A2>
	T* alloc(A1 a1, A2 a2) { return alloc_real(new T(a1, a2)); }

	template <typename T, typename A1, typename A2, typename A3>
	T* alloc(A1 a1, A2 a2, A3 a3) { return alloc_real(new T(a1, a2, a3)); }

	template <typename T>
	static unsigned int object_parse(void* data, int argc, const char** argv)
	{
		return (*reinterpret_cast<T*>(data))(argc, argv);
	}

	static void raise(const char* fmt, ...)
	__attribute__((noreturn))
	__attribute__((format(printf, 1, 2)));

private:
	typedef std::vector< std::pair<void (*)(void*), void*> > pool_t;
	pool_t m_pool;

	template <typename T> static void object_delete(void* data)
	{
		delete reinterpret_cast<T*>(data);
	}

	template <typename T> T* alloc_real(T* new_data)
	{
		std::auto_ptr<T> data(new_data);
		m_pool.push_back( pool_t::value_type(
					&parser::object_delete<T>,
					reinterpret_cast<void*>(data.get())
					) );
		return data.release();
	}

public:
	parser() { }

	~parser()
	{
		for(pool_t::iterator it(m_pool.begin());
				it != m_pool.end(); ++it) {
			(*it->first)(it->second);
		}
	}
};

extern std::auto_ptr<parser> s_parser;


namespace convert {

#if 0
// convert network interface name to IPv4 address
int netif_addr4(sockaddr_in* dst,  const char* ifname, unsigned short port);

// convert network interface name to IPv6 address
int netif_addr6(sockaddr_in6* dst, const char* ifname, unsigned short port);

// convert network interface name to IPv4 address or IPv6 address
int netif_addr(sockaddr_in6* dst,  const char* ifname, unsigned short port);
#endif

template <typename T>
bool numeric(T* dst, const char* str)
{
	std::istringstream stream(str);
	stream >> *dst;
	if(stream.fail() || stream.bad() || !stream.eof()) {
		return false;
	}
	return true;
}

}  // namespace convert


namespace type {

namespace detail {
	template <typename T>
	static unsigned int parse_numeric(void* dst, int argc, const char** argv) {
		if(argc < 1) { parser::raise("numeric value is required."); }
		if(!convert::numeric((T*)dst, argv[0])) {
#ifndef MP_NO_CXX_ABI_H
			int status;
			parser::raise("%s is expected: %s",
					abi::__cxa_demangle(typeid(T).name(), 0, 0, &status),
					argv[0]);
#else
			parser::raise("invalid number: %s", argv[0]);
#endif
		}
		return 1;
	}
}  // namespace detail

template <typename T> acceptable numeric(T* dst)
{
	return acceptable(&detail::parse_numeric<T>, (void*)dst, true);
}

template <typename T> acceptable numeric(T* dst, T d)
{
	*dst = d;
	return acceptable(&detail::parse_numeric<T>, (void*)dst, false);
}

}  // namespace type


}  // namespace kazuhiki

#endif /* kazuhiki.h */

