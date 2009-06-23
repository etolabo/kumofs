#ifndef RPC_EXCEPTION_H__
#define RPC_EXCEPTION_H__

#include <exception>

namespace rpc {


struct connection_error : public std::exception {
	connection_error() { }
};

struct connection_closed_error : public connection_error {
	connection_closed_error() { }
	const char* what() const throw() { return "connection closed"; }
};

struct connection_broken_error : public connection_error {
	connection_broken_error() { }
	const char* what() const throw() { return "connection broken"; }
};


}  // namespace rpc

#endif /* rpc/exception.h */

