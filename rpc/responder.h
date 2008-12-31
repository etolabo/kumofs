#ifndef RPC_RESPONDER_H__
#define RPC_RESPONDER_H__

#include "log/mlogger.h" //FIXME
#include "rpc/types.h"
#include "rpc/sbuffer.h"
#include "rpc/vrefbuffer.h"
#include "rpc/wavy.h"

namespace rpc {


class responder {
public:
	responder(int fd, msgid_t msgid);

	~responder();

	template <typename Result>
	void result(Result res);

	template <typename Result>
	void result(Result res, auto_zone z);

	template <typename Error>
	void error(Error err);

	template <typename Error>
	void error(Error err, auto_zone z);

	void null();

	void send_response(const char* buf, size_t buflen, auto_zone z);

	void send_responsev(const struct iovec* vb, size_t count, auto_zone z);

private:
	template <typename Result, typename Error>
	void call(Result& res, Error& err);

	template <typename Result, typename Error>
	void call(Result& res, Error& err, auto_zone z);

private:
	int m_fd;
	msgid_t m_msgid;

private:
	responder();
};


}  // namespace rpc

#endif /* rpc/responder.h */

