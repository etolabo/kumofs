//
// mp::wavy::output
//
// Copyright (C) 2008 FURUHASHI Sadayuki
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

#ifndef MP_WAVY_OUTPUT_H__
#define MP_WAVY_OUTPUT_H__

#include <memory>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

namespace mp {
namespace wavy {


class output {
public:
	output();
	~output();

	void add_thread(size_t num);

	void end();

	void join();
	void detach();

public:
	typedef void (*finalize_t)(void* user);
	struct request {
		request() : finalize(NULL), user(NULL) { }
		request(finalize_t f, void* u) : finalize(f), user(u) { }
		finalize_t finalize;
		void* user;
	};

	void write(int fd, const char* buf, size_t buflen);
	void writev(int fd, const iovec* vec, size_t veclen);

	void write(int fd, const char* buf, size_t buflen, request req);
	void write(int fd, const char* buf, size_t buflen, finalize_t finalize, void* user);
	void writev(int fd, const iovec* vec, size_t veclen, request req);
	void writev(int fd, const iovec* vec, size_t veclen, finalize_t finalize, void* user);

	void writev(int fd, const iovec* bufvec, const request* reqvec, size_t veclen);

private:
	class impl;
	const std::auto_ptr<impl> m_impl;

	output(const output&);
};


}  // namespace wavy
}  // namespace mp

#endif /* mp/wavy/output.h */

