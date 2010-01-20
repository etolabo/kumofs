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

#include "kazuhiki.h"
#include <map>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

namespace kazuhiki {


struct invalid_argument_real : public invalid_argument {
	invalid_argument_real(const std::string& msg) :
		invalid_argument(msg) { }

	virtual ~invalid_argument_real() throw() { }

	virtual const char* what() const throw()
	{
		if(msg.empty()) {
			return std::runtime_error::what();
		} else {
			return msg.c_str();
		}
	}

	void setkey(const std::string& k) throw()
	{
		msg = std::string("argument error `") + k + "': " + what();
	}

private:
	std::string msg;
};


void parser::raise(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	char buf[256];
	vsnprintf(buf, sizeof(buf), fmt, ap);

	va_end(ap);
	throw invalid_argument_real(buf);
}


class command {
public:
	command() { }
	~command() { }

private:
	static int parse_through(int& argc, const char** argv, int i, int s)
	{
		return i + s;
	}

	static int parse_break(int& argc, const char** argv, int i, int s)
	{
		memmove(argv + i, argv + i + s, (argc - i - s)*sizeof(char*));
		argc -= s;
		return i;
	}

	static int fail_through(int& argc, const char** argv, int i)
	{
		return i + 1;
	}

	static int fail_exception(int& argc, const char** argv, int i)
	{
		throw unknown_argument(std::string("unexpected argument: ")+argv[i]);
	}

	template < int (*parse_method)(int& argc, const char** argv, int i, int s),
		   int (*fail_method)(int& argc, const char** argv, int i) >
	void parse_real(int& argc, const char** argv)
	{
		int i = 0;
		while(i < argc) {
			std::string key(argv[i]);
			try {
				map_t::iterator it(m_map.find(key));
				if(it != m_map.end()) {
					acceptable& ac(it->second.ac);
					unsigned int s = (*ac.parse)(ac.data, argc - i - 1, argv + i + 1);
					i = (*parse_method)(argc, argv, i, s + 1);
					*it->second.shared_required = false;
					if(it->second.optional) {
						*it->second.optional = true;
					}
				} else {
					i = (*fail_method)(argc, argv, i);
				}
			} catch (invalid_argument_real& e) {
				e.setkey(key);
				throw;
			}
		}

		std::vector<std::string> missing;
		for(map_t::reverse_iterator it(m_map.rbegin()), it_end(m_map.rend());
				it != it_end; ++it) {
			entry& e(it->second);
			if(*e.shared_required && e.optional == NULL) {
				missing.push_back(it->first);
				*e.shared_required = false;
			}
		}
		if(!missing.empty()) {
			std::string msg("required but not set: ");
			std::vector<std::string>::iterator it(missing.begin());
			msg += *it;
			++it;
			for(; it != missing.end(); ++it) {
				msg += ", ";
				msg += *it;
			}
			throw invalid_argument_real(msg);
		}
	}

public:
	void on(const char* short_name, const char* long_name,
			bool* optional, acceptable ac)
	{
		entry e;
		e.ac = ac;
		e.shared_required = s_parser->alloc<bool>(ac.required);
		e.optional = optional;
		if(e.optional) { *e.optional = false; }
		if(short_name) {
			m_map[std::string(short_name)] = e;
		}
		if(long_name) {
			m_map[std::string(long_name)] = e;
		}
	}

	void parse(int argc, const char** argv)
	{
		parse_real<&command::parse_through, &command::fail_through>(argc, argv);
	}

	void break_parse(int& argc, const char** argv)
	{
		parse_real<&command::parse_break, &command::fail_through>(argc, argv);
	}

	void order(int argc, const char** argv)
	{
		parse_real<&command::parse_through, &command::fail_exception>(argc, argv);
	}

	void break_order(int& argc, const char** argv)
	{
		parse_real<&command::parse_break, &command::fail_exception>(argc, argv);
	}

private:
	struct entry {
		bool* shared_required;
		bool* optional;
		acceptable ac;
	};
	typedef std::map<std::string, entry> map_t;
	map_t m_map;
};

static command* cmd;

std::auto_ptr<parser> s_parser;


void init()
{
	s_parser.reset( new parser() );
	cmd = s_parser->alloc<command>();
}

void on(const char* short_name, const char* long_name, acceptable ac)
{
	cmd->on(short_name, long_name, NULL, ac);
}

void on(const char* short_name, const char* long_name, bool* optional, acceptable ac)
{
	cmd->on(short_name, long_name, optional, ac);
}

void parse(int argc, char** argv)
{
	cmd->parse(argc, (const char**)argv);
	s_parser.reset();
}

void break_parse(int& argc, char** argv)
{
	cmd->break_parse(argc, (const char**)argv);
	s_parser.reset();
}

void order(int argc, char** argv)
{
	cmd->order(argc, (const char**)argv);
	s_parser.reset();
}

void break_order(int& argc, char** argv)
{
	cmd->break_order(argc, (const char**)argv);
	s_parser.reset();
}


acceptable::acceptable() :
	parse(NULL), data(NULL), required(false) { }

acceptable::acceptable(parse_t p, void* d, bool r) :
	parse(p), data(d), required(r) { }


namespace type {


static unsigned int parse_boolean(bool* dst, int argc, const char** argv)
{
	if(argc > 0) {
		if(		strcmp("true", argv[0]) == 0 ||
				strcmp("yes",  argv[0]) == 0 ||
				strcmp("on",   argv[0]) == 0 ) {
			*dst = true;
			return 1;
		} else if(
				strcmp("false", argv[0]) == 0 ||
				strcmp("no",    argv[0]) == 0 ||
				strcmp("off",   argv[0]) == 0 ) {
			*dst = false;
			return 1;
		} else {
			*dst = true;
			return 0;
		}
	} else {
		*dst = true;
		return 0;
	}
}

acceptable boolean(bool* dst)
{
	*dst = false;
	return acceptable((parse_t)parse_boolean, (void*)dst, false);
}


static unsigned int parse_string(std::string* dst, int argc, const char** argv)
{
	if(argc < 1) { parser::raise("string is required."); }
	*dst = argv[0];
	return 1;
}

acceptable string(std::string* dst)
{
	return acceptable((parse_t)parse_string, dst, true);
}

acceptable string(std::string* dst, const std::string& d)
{
	*dst = d;
	return acceptable((parse_t)parse_string, dst, false);
}


template <typename Address, bool UseIPv6>
struct parse_network_base {
	parse_network_base(Address* dst) :
		m_dst(dst), m_port(0) { }

	parse_network_base(Address* dst, unsigned short port) :
		m_dst(dst), m_port(port) { }

protected:
	void resolve_port(const char* port = NULL)
	{
		if(port) {
			if(!convert::numeric(&m_port, port)) {
				parser::raise("invalid port number: %s", port);
			}
		} else if(m_port == 0) {
			parser::raise("port number is required.");
		}
	}

	void resolve_addr(const char* host, const char* port = NULL)
	{
		memset(m_dst, 0, sizeof(Address));
		resolve_port(port);

		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = UseIPv6 ? AF_UNSPEC : AF_INET;
		hints.ai_socktype = SOCK_STREAM;  // FIXME
#ifdef __FreeBSD__
		hints.ai_flags = AI_ADDRCONFIG;
#else
		hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
#endif

		addrinfo *res = NULL;
		int err;
		if( (err=getaddrinfo(host, NULL, &hints, &res)) != 0 ) {
			parser::raise("can't resolve host name (%s): %s", gai_strerror(err), host);
		}

		for(addrinfo* rp=res; rp; rp = rp->ai_next) {
			if(rp->ai_family == AF_INET ||
					(UseIPv6 && rp->ai_family == AF_INET6)) {
				memcpy((void*)m_dst, (const void*)rp->ai_addr,
						std::max((size_t)rp->ai_addrlen, (size_t)sizeof(Address)));
				((sockaddr_in*)m_dst)->sin_port = htons(m_port);
				freeaddrinfo(res);
				return;
			}
		}

		parser::raise("can't resolve host name (no suitable family): %s", host);
	}

	void addr_any(unsigned short port, bool dummy)
	{
		memset(m_dst, 0, sizeof(Address));
		//if(typeid(Address) == typeid(sockaddr_in)) {
			sockaddr_in* addr = (sockaddr_in*)m_dst;
			addr->sin_family = AF_INET;
			addr->sin_port = htons(port);
			addr->sin_addr.s_addr = INADDR_ANY;
		//} else {
		//	sockaddr_in6* addr = (sockaddr_in6*)m_dst;
		//	addr->sin6_family = AF_INET6;
		//	addr->sin6_port = htons(port);
		//	addr->sin6_addr = in6addr_any;
		//}
	}

	void addr_any(const char* port = NULL)
	{
		resolve_port(port);
		addr_any(m_port, true);
	}

	void addr_path(const char* path)
	{
		sockaddr_un* addr = (sockaddr_un*)m_dst;
		if(strlen(path) > sizeof(addr->sun_path)) {
			parser::raise("path too long: %s", path);
		}
		addr->sun_family = AF_UNIX;
		strcpy(addr->sun_path, path);
	}

	Address* m_dst;
	unsigned short m_port;
};


template <typename Address, bool UseIPv6>
struct parse_connectable : parse_network_base<Address, UseIPv6> {
	typedef parse_network_base<Address, UseIPv6> net;

	parse_connectable(Address* dst) : net(dst)
		{ memset(dst, 0, sizeof(Address)); }

	parse_connectable(Address* dst, unsigned short port) : net(dst, port)
		{ memset(dst, 0, sizeof(Address)); }

	unsigned int operator() (int argc, const char** argv)
	{
		if(argc < 1) { parser::raise("network address is required."); }
		std::string str(argv[0]);

		if(typeid(Address) == typeid(sockaddr_un) &&
				str.find('/') != std::string::npos) {
			net::addr_path(argv[0]);
			return 1;
		}

		std::string::size_type posc = str.rfind(':');
		if(posc != std::string::npos) {
			if(UseIPv6 && str.find(':') != posc) {
				// IPv6 address
				if( posc != 0 && str[posc-1] == ']' && str[0] == '[' ) {
					// [ip:add::re:ss]:port
					std::string host( str.substr(1,posc-2) );
					std::string port( str.substr(posc+1) );
					net::resolve_addr(host.c_str(), port.c_str());
					return 1;
				} else {
					// ip:add::re:ss  (default port)
					net::resolve_addr(str.c_str(), NULL);
					return 1;
				}
			} else {
				// host:port
				// ip.add.re.ss:port
				std::string host( str.substr(0,posc) );
				std::string port( str.substr(posc+1) );
				net::resolve_addr(host.c_str(), port.c_str());
				return 1;
			}
		} else {
			// host           (default port)
			// ip.add.re.ss   (default port)
			net::resolve_addr(str.c_str(), NULL);
			return 1;
		}
	}
};


template <typename Address, bool UseIPv6>
struct parse_listenable : parse_network_base<Address, UseIPv6> {
	typedef parse_network_base<Address, UseIPv6> net;

	parse_listenable(Address* dst) : net(dst)
		{ net::addr_any(0, true); }

	parse_listenable(Address* dst, unsigned short port) : net(dst, port)
		{ net::addr_any(port, true); }

	unsigned int operator() (int argc, const char** argv)
	{
		if(argc < 1) { parser::raise("network address is required."); }
		std::string str(argv[0]);
		std::string::size_type posc = str.rfind(':');
		if(posc != std::string::npos) {
			if(UseIPv6 && str.find(':') != posc) {
				// IPv6 address
				if( posc != 0 && str[posc-1] == ']' && str[0] == '[' ) {
					// [ip:add::re:ss]:port
					std::string host( str.substr(1,posc-2) );
					std::string port( str.substr(posc+1) );
					net::resolve_addr(host.c_str(), port.c_str());
					return 1;
				} else {
					// ip::add::re:ss  (default port)
					net::resolve_addr(str.c_str(), NULL);
					return 1;
				}
			} else {
				std::string host( str.substr(0,posc) );
				std::string port( str.substr(posc+1) );
				if(host.empty()) {
					// :port
					net::addr_any(port.c_str());
					return 1;
				} else {
					// host:port
					net::resolve_addr(host.c_str(), port.c_str());
					return 1;
				}
			}
		} else {
			unsigned short tmp;
			if(convert::numeric(&tmp, str.c_str())) {
				// port
				net::addr_any(str.c_str());
				return 1;
			} else {
				// host         (default port)
				net::resolve_addr(str.c_str(), NULL);
				return 1;
			}
		}
	}
};


acceptable connectable(sockaddr_in*      dst)
{
	typedef parse_connectable<sockaddr_in, false> type;
	return acceptable(
			&parser::object_parse<type>,
			(void*)s_parser->alloc<type>(dst),
			true);
}

acceptable connectable(sockaddr_in6*     dst)
{
	typedef parse_connectable<sockaddr_in6, true> type;
	return acceptable(
			&parser::object_parse<type>,
			(void*)s_parser->alloc<type>(dst),
			true);
}

acceptable connectable(sockaddr_in*      dst, unsigned short d_port)
{
	typedef parse_connectable<sockaddr_in, false> type;
	return acceptable(
			&parser::object_parse<type>,
			(void*)s_parser->alloc<type>(dst, d_port),
			true);
}

acceptable connectable(sockaddr_in6*     dst, unsigned short d_port)
{
	typedef parse_connectable<sockaddr_in6, true> type;
	return acceptable(
			&parser::object_parse<type>,
			(void*)s_parser->alloc<type>(dst, d_port),
			true);
}


acceptable listenable(sockaddr_in*      dst)
{
	typedef parse_listenable<sockaddr_in, true> type;
	return acceptable(
			&parser::object_parse<type>,
			(void*)s_parser->alloc<type>(dst),
			true);
}

acceptable listenable(sockaddr_in6*     dst)
{
	typedef parse_listenable<sockaddr_in6, true> type;
	return acceptable(
			&parser::object_parse<type>,
			(void*)s_parser->alloc<type>(dst),
			true);
}

acceptable listenable(sockaddr_in*      dst, unsigned short d_port)
{
	typedef parse_listenable<sockaddr_in, true> type;
	return acceptable(
			&parser::object_parse<type>,
			(void*)s_parser->alloc<type>(dst, d_port),
			false);
}

acceptable listenable(sockaddr_in6*     dst, unsigned short d_port)
{
	typedef parse_listenable<sockaddr_in6, true> type;
	return acceptable(
			&parser::object_parse<type>,
			(void*)s_parser->alloc<type>(dst, d_port),
			false);
}


}  // namespace type


namespace convert {

static int netif_addr_impl(const char* ifname, unsigned short port, void* dst, int family)
{
	struct ifaddrs* ifap;
	if(getifaddrs(&ifap)) { return -1; }
	int ret = -1;
	for(struct ifaddrs* i=ifap; i != NULL; i = i->ifa_next) {
		if(i->ifa_addr == NULL) { continue; }
		if(strcmp(ifname, i->ifa_name) != 0) { continue; }
		int sa_family = i->ifa_addr->sa_family;
		if(sa_family == AF_INET && (family == AF_INET || family == AF_UNSPEC)) {
			memcpy(dst, i->ifa_addr, sizeof(sockaddr_in));
			((struct sockaddr_in*)dst)->sin_port = htons(port);
			ret = 0;
			break;
		} else if(sa_family == AF_INET6 && (family == AF_INET6 || family == AF_UNSPEC)) {
			memcpy(dst, i->ifa_addr, sizeof(sockaddr_in6));
			((struct sockaddr_in6*)dst)->sin6_port = htons(port);
			ret = 0;
			break;
		}
	}
	freeifaddrs(ifap);
	return ret;
}

int netif_addr4(const char* ifname, unsigned short port, sockaddr_in* dst)
{
	memset(dst, 0, sizeof(sockaddr_in));
	return netif_addr_impl(ifname, port, dst, AF_INET);
}

int netif_addr6(const char* ifname, unsigned short port, sockaddr_in6* dst)
{
	memset(dst, 0, sizeof(sockaddr_in6));
	return netif_addr_impl(ifname, port, dst, AF_INET6);
}

int netif_addr(const char* ifname, unsigned short port, sockaddr_in6* dst)
{
	memset(dst, 0, sizeof(sockaddr_in6));
	return netif_addr_impl(ifname, port, dst, AF_UNSPEC);
}

}  // namespace convert


}  // namespace kazuhiki

