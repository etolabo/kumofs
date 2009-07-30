//
// kumofs
//
// Copyright (C) 2009 Etolabo Corp.
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

