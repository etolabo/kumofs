//
// mp::object_callback
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

#ifndef MP_OBJECT_CALLBACK_H__
#define MP_OBJECT_CALLBACK_H__

#include "mp/memory.h"

namespace mp {


template <typename T>
static void object_destructor(void* obj)
{
	reinterpret_cast<T*>(obj)->~T();
}


template <typename T>
static void object_delete(void* obj)
{
	delete reinterpret_cast<T*>(obj);
}


template <typename Signature>
struct object_callback;

template <typename R>
struct object_callback<R ()>
{
	template <typename T, R (T::*MemFun)()>
	static R mem_fun(void* obj)
	{
		return (reinterpret_cast<T*>(obj)->*MemFun)();
	}

	template <typename T, R (T::*MemFun)()>
	static R const_mem_fun(const void* obj)
	{
		return (reinterpret_cast<const T*>(obj)->*MemFun)();
	}

	template <typename T, R (T::*MemFun)()>
	static R shared_fun(shared_ptr<T> obj)
	{
		return (obj.get()->*MemFun)();
	}
};

MP_ARGS_BEGIN
template <typename R, MP_ARGS_TEMPLATE>
struct object_callback<R (MP_ARGS_PARAMS)>
{
	template <typename T, R (T::*MemFun)(MP_ARGS_PARAMS)>
	static R mem_fun(void* obj, MP_ARGS_PARAMS)
	{
		return (reinterpret_cast<T*>(obj)->*MemFun)(MP_ARGS_FUNC);
	}

	template <typename T, R (T::*MemFun)(MP_ARGS_PARAMS)>
	static R const_mem_fun(const void* obj, MP_ARGS_PARAMS)
	{
		return (reinterpret_cast<const T*>(obj)->*MemFun)(MP_ARGS_FUNC);
	}

	template <typename T, R (T::*MemFun)(MP_ARGS_PARAMS)>
	static R shared_fun(shared_ptr<T> obj, MP_ARGS_PARAMS)
	{
		return (obj.get()->*MemFun)(MP_ARGS_FUNC);
	}
};

MP_ARGS_END

}  // namespace mp

#endif /* mp/object_callback.h */

