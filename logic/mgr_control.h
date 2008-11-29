#ifndef LOGIC_MGR_CONTROL_H__
#define LOGIC_MGR_CONTROL_H__

#include <msgpack.hpp>
#include "logic/hash.h"
#include "rpc/message.h"

namespace kumo {

namespace control {

using msgpack::define;
using msgpack::type::tuple;
using msgpack::type::raw_ref;
typedef HashSpace::Seed HSSeed;

enum command_type {
	GetStatus			= 84,
	AttachNewServers   	= 85,
	DetachFaultServers	= 86,
	CreateBackup		= 87,
	SetAutoReplace      = 88,
};

namespace type {
	struct Status : define< tuple<
			HSSeed,
			std::vector<address> > > {
		HSSeed& hsseed() { return get<0>(); }
		std::vector<address>& newcomers() { return get<1>(); }
	};

	struct GetStatus : define< tuple<> > {
		GetStatus() { }
	};

	struct AttachNewServers : define< tuple<bool> > {
		AttachNewServers() { }
		AttachNewServers(bool replace) :
			define_type(msgpack_type( replace )) { }
		bool replace() const		{ return get<0>(); }
	};

	struct DetachFaultServers : define< tuple<bool> > {
		DetachFaultServers() { }
		DetachFaultServers(bool replace) :
			define_type(msgpack_type( replace )) { }
		bool replace() const		{ return get<0>(); }
	};

	struct CreateBackup : define< tuple<std::string> > {
		CreateBackup() { }
		CreateBackup(const std::string& suffix) :
			define_type(msgpack_type( suffix )) { }
		const std::string& suffix() const	{ return get<0>(); }
	};

	struct SetAutoReplace : define< tuple<bool> > {
		SetAutoReplace() { }
		SetAutoReplace(bool enable) :
			define_type(msgpack_type( enable )) { }
		bool enable() const			{ return get<0>(); }
	};

	struct Error : define< tuple<std::string> > {
		Error() { }
		Error(const std::string& msg) :
			define_type(msgpack_type( msg ) ) { }
		const std::string& message() const { return get<0>(); }
	};
}


#define CONTROL_DECL(NAME) \
	void NAME(control::type::NAME, rpc::responder)

#define CONTROL_IMPL(NAME, param, response) \
	void Manager::NAME(control::type::NAME param, rpc::responder response)


}  // namespace control

}  // namespace kumo

#endif /* logic/mgr_control.h */

