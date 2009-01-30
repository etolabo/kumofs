//
// mp::functional
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

#ifndef MP_FUNCTIONAL_H__
#define MP_FUNCTIONAL_H__

#ifdef MP_FUNCTIONAL_BOOST
#include <boost/tr1/functional.hpp>
namespace mp {
	using std::tr1::function;
	using std::tr1::bind;
	namespace placeholders {
		using namespace std::tr1::placeholders;
	}
}
#else
#ifdef MP_FUNCTIONAL_BOOST_ORG
#include <boost/function.hpp>
#include <boost/bind.hpp>
namespace mp {
	using boost::function;
	using boost::bind;
	namespace placeholders { }
}
#else
#ifndef MP_FUNCTIONAL_STANDARD
#include <tr1/functional>
namespace mp {
	using std::tr1::function;
	using std::tr1::bind;
	namespace placeholders {
		using namespace std::tr1::placeholders;
	}
}
#else
#include <functional>
namespace mp {
	using std::function;
	using std::bind;
	namespace placeholders {
		using namespace std::placeholders;
	}
}
#endif
#endif
#endif

#endif /* mp/functional.h */

