#ifndef RPC_WEAK_RESPONDER_FWD_H__
#define RPC_WEAK_RESPONDER_FWD_H__

#include "rpc/types.h"

namespace rpc {


class weak_responder {
public:
	weak_responder(basic_weak_session s, msgid_t msgid);

	~weak_responder();

	template <typename Result>
	void result(Result res);

	template <typename Result>
	void result(Result res, shared_zone& life);

	template <typename Error>
	void error(Error err);

	template <typename Error>
	void error(Error err, shared_zone& life);

	void null();

private:
	template <typename Result, typename Error>
	void call(Result& res, Error& err);

	template <typename Result, typename Error>
	void call(Result& res, Error& err, shared_zone& life);

private:
	basic_weak_session m_session;
	const msgid_t m_msgid;

private:
	weak_responder();
};


}  // namespace rpc

#endif /* rpc/weak_responder_fwd.h */

