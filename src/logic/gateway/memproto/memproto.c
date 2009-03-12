/*
 * memproto  memcached binary protocol parser
 *
 * Copyright (C) 2008 FURUHASHI Sadayuki
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "memproto.h"
#include <string.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define __LITTLE_ENDIAN__
#elif __BYTE_ORDER == __BIG_ENDIAN
#define __BIG_ENDIAN__
#endif
#endif

#ifdef __LITTLE_ENDIAN__
#if defined(__bswap_64)
#  define memproto_be64h(x) __bswap_64(x)
#elif defined(__DARWIN_OSSwapInt64)
#  define memproto_be64h(x) __DARWIN_OSSwapInt64(x)
#else
static inline uint64_t memproto_be64h(uint64_t x) {
	return	((x << 56) & 0xff00000000000000ULL ) |
			((x << 40) & 0x00ff000000000000ULL ) |
			((x << 24) & 0x0000ff0000000000ULL ) |
			((x <<  8) & 0x000000ff00000000ULL ) |
			((x >>  8) & 0x00000000ff000000ULL ) |
			((x >> 24) & 0x0000000000ff0000ULL ) |
			((x >> 40) & 0x000000000000ff00ULL ) |
			((x >> 56) & 0x00000000000000ffULL ) ;
}
#endif
#else
#define memproto_be64h(x) (x)
#endif

#define MEMPROTO_MAGIC(header)             (*(( uint8_t*)&((const char*)header)[0]))
#define MEMPROTO_OPCODE(header)            (*(( uint8_t*)&((const char*)header)[1]))
#define MEMPROTO_KEY_LENGTH(header)   ntohs(*((uint16_t*)&((const char*)header)[2]))
#define MEMPROTO_EXTRA_LENGTH(header)      (*(( uint8_t*)&((const char*)header)[4]))
#define MEMPROTO_DATA_TYPE(header)         (*(( uint8_t*)&((const char*)header)[5]))
#define MEMPROTO_STATUS(header)       ntohs(*((uint16_t*)&((const char*)header)[6]))
#define MEMPROTO_BODY_LENGTH(header)  ntohl(*((uint32_t*)&((const char*)header)[8]))
#define MEMPROTO_OPAQUE(header)       ntohl(*((uint32_t*)&((const char*)header)[12]))
#define MEMPROTO_CAS(header)          memproto_be64h(*((uint64_t*)&((const char*)header)[16]))

#define MEMPROTO_CALLBACK(cb, ...)        ((void (*)(__VA_ARGS__))cb)
#define MEMPROTO_EXTRA_4_EXPIRATION(extra)   htonl(*((uint32_t*)&extra[0]))
#define MEMPROTO_EXTRA_8_FLAGS(extra)        htonl(*((uint32_t*)&extra[0]))
#define MEMPROTO_EXTRA_8_EXPIRATION(extra)   htonl(*((uint32_t*)&extra[4]))
#define MEMPROTO_EXTRA_20_AMOUNT(extra)      memproto_be64h(*((uint64_t*)&extra[0]))
#define MEMPROTO_EXTRA_20_INITIAL(extra)     memproto_be64h(*((uint64_t*)&extra[8]))
#define MEMPROTO_EXTRA_20_EXPIRATION(extra)  memproto_be64h(*((uint32_t*)&extra[16]))


void memproto_parser_init(memproto_parser* ctx, memproto_callback* cb, void* user)
{
	memset(ctx, 0, sizeof(memproto_parser));
	ctx->callback[0x00] = (void*)cb->cb_get;
	ctx->callback[0x01] = (void*)cb->cb_set;
	ctx->callback[0x02] = (void*)cb->cb_add;
	ctx->callback[0x03] = (void*)cb->cb_replace;
	ctx->callback[0x04] = (void*)cb->cb_delete;
	ctx->callback[0x05] = (void*)cb->cb_increment;
	ctx->callback[0x06] = (void*)cb->cb_decrement;
	ctx->callback[0x07] = (void*)cb->cb_quit;
	ctx->callback[0x08] = (void*)cb->cb_flush;
	ctx->callback[0x09] = (void*)cb->cb_getq;
	ctx->callback[0x0a] = (void*)cb->cb_noop;
	ctx->callback[0x0b] = (void*)cb->cb_version;
	ctx->callback[0x0c] = (void*)cb->cb_getk;
	ctx->callback[0x0d] = (void*)cb->cb_getkq;
	ctx->callback[0x0e] = (void*)cb->cb_append;
	ctx->callback[0x0f] = (void*)cb->cb_prepend;
	ctx->user = user;
}


int memproto_parser_execute(memproto_parser* ctx, const char* data, size_t datalen, size_t* off)
{
	size_t region = datalen - *off;

	if(region < MEMPROTO_HEADER_SIZE) { return 0; }

	ctx->header = data + *off;
	uint32_t bodylen = MEMPROTO_BODY_LENGTH(ctx->header);

	region -= MEMPROTO_HEADER_SIZE;
	if(region < bodylen) { return 0; }

	if( MEMPROTO_MAGIC(ctx->header) != MEMPROTO_REQUEST ) { return -1; }

	*off += MEMPROTO_HEADER_SIZE + bodylen;

	return 1;
}


int memproto_dispatch(memproto_parser* ctx)
{
	memproto_header h;

	h.magic                 = MEMPROTO_MAGIC(ctx->header);
	h.opcode                = MEMPROTO_OPCODE(ctx->header);
	const uint16_t keylen   = MEMPROTO_KEY_LENGTH(ctx->header);
	const uint16_t extralen = MEMPROTO_EXTRA_LENGTH(ctx->header);
	h.data_type             = MEMPROTO_DATA_TYPE(ctx->header);
	h.reserved              = MEMPROTO_STATUS(ctx->header);
	const uint32_t bodylen  = MEMPROTO_BODY_LENGTH(ctx->header);
	h.opaque                = MEMPROTO_OPAQUE(ctx->header);
	h.cas                   = MEMPROTO_CAS(ctx->header);
	const char* const extra = ctx->header + MEMPROTO_HEADER_SIZE;
	const char* const key   = extra + extralen;
	const char* const val   = key + keylen;
	const uint32_t vallen   = bodylen - extralen - keylen;

	memproto_command cmd = (memproto_command)h.opcode;
	void* cb = ctx->callback[cmd];
	if(!cb) { return -cmd; }

	switch(cmd) {
	case MEMPROTO_CMD_GET:
	case MEMPROTO_CMD_GETQ:
	case MEMPROTO_CMD_GETK:
	case MEMPROTO_CMD_GETKQ:
		if(extralen != 0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(vallen   != 0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(keylen   == 0) { return MEMPROTO_INVALID_ARGUMENT; }
		MEMPROTO_CALLBACK(cb, void*, memproto_header*,
				const char*, uint16_t)(ctx->user, &h,
					key, keylen);
		return 1;

	case MEMPROTO_CMD_DELETE:
		if(keylen   == 0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(vallen   != 0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(extralen == 0) {
			MEMPROTO_CALLBACK(cb, void*, memproto_header*,
					const char*, uint16_t,
					uint32_t)(ctx->user, &h,
						key, keylen,
						0);
			return 1;
		} else if(extralen == 4) {
			MEMPROTO_CALLBACK(cb, void*, memproto_header*,
					const char*, uint16_t,
					uint32_t)(ctx->user, &h,
						key, keylen,
						MEMPROTO_EXTRA_4_EXPIRATION(extra));
			return 1;
		} else { return MEMPROTO_INVALID_ARGUMENT; }

	case MEMPROTO_CMD_FLUSH:
		if(keylen   != 0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(vallen   != 0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(extralen == 0) {
			MEMPROTO_CALLBACK(cb, void*, memproto_header*,
					uint32_t)(ctx->user, &h,
						0);
			return 1;
		} else if(extralen == 4) {
			MEMPROTO_CALLBACK(cb, void*, memproto_header*,
					uint32_t)(ctx->user, &h,
						MEMPROTO_EXTRA_4_EXPIRATION(extra));
			return 1;
		} else { return MEMPROTO_INVALID_ARGUMENT; }


	case MEMPROTO_CMD_SET:
	case MEMPROTO_CMD_ADD:
	case MEMPROTO_CMD_REPLACE:
		if(keylen   == 0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(extralen != 8) { return MEMPROTO_INVALID_ARGUMENT; }
		/*if(vallen   == 0) { return MEMPROTO_INVALID_ARGUMENT; }*/
		MEMPROTO_CALLBACK(cb, void*, memproto_header*,
				const char*, uint16_t,
				const char*, uint32_t,
				uint32_t, uint32_t)(ctx->user, &h,
					key, keylen,
					val, vallen,
					MEMPROTO_EXTRA_8_FLAGS(extra),
					MEMPROTO_EXTRA_8_EXPIRATION(extra));
		return 1;

	case MEMPROTO_CMD_INCREMENT:
	case MEMPROTO_CMD_DECREMENT:
		if(extralen != 20) { return MEMPROTO_INVALID_ARGUMENT; }
		if(keylen   ==  0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(vallen   !=  0) { return MEMPROTO_INVALID_ARGUMENT; }
		MEMPROTO_CALLBACK(cb, void*, memproto_header*,
				const char*, uint16_t,
				uint64_t, uint64_t, uint32_t)(ctx->user, &h,
					key, keylen,
					MEMPROTO_EXTRA_20_AMOUNT(extra),
					MEMPROTO_EXTRA_20_INITIAL(extra),
					MEMPROTO_EXTRA_20_EXPIRATION(extra));
		return 1;

	case MEMPROTO_CMD_QUIT:
	case MEMPROTO_CMD_NOOP:
	case MEMPROTO_CMD_VERSION:
		if(keylen   != 0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(extralen != 0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(vallen   != 0) { return MEMPROTO_INVALID_ARGUMENT; }
		MEMPROTO_CALLBACK(cb, void*, memproto_header*)(ctx->user, &h);
		return 1;

	case MEMPROTO_CMD_APPEND:
	case MEMPROTO_CMD_PREPEND:
		if(keylen   == 0) { return MEMPROTO_INVALID_ARGUMENT; }
		if(extralen != 0) { return MEMPROTO_INVALID_ARGUMENT; }
		/*if(vallen   == 0) { return MEMPROTO_INVALID_ARGUMENT; }*/
		MEMPROTO_CALLBACK(cb, void*, memproto_header*,
				const char*, uint16_t,
				const char*, uint32_t)(ctx->user, &h,
					key, keylen,
					val, vallen);
		return 1;
	}

	return -cmd;
}


#ifdef __cplusplus
}
#endif

