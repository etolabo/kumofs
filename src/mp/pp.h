#ifndef MP_PP_H__
#define MP_PP_H__

#define MP_PP_STR(s) #s
#define MP_PP_XSTR(s) MP_PP_STR(s)
#define MP_PP_CONCAT(a,b)  a##b
#define MP_PP_XCONCAT(a,b) MP_PP_CONCAT(a,b)
#define MP_PP_HEADER(dir, prefix, file, suffix) \
		MP_PP_XSTR( \
			MP_PP_XCONCAT(dir/prefix ## file, suffix).h \
			)

#endif /* mp/pp.h */

