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

#ifndef MEMPROTO_H__
#define MEMPROTO_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	MEMPROTO_REQUEST                = 0x80,
	MEMPROTO_RESPONSE               = 0x81,
} memproto_magic;


typedef enum {
	MEMPROTO_RES_NO_ERROR           = 0x0000,
	MEMPROTO_RES_KEY_NOT_FOUND      = 0x0001,
	MEMPROTO_RES_KEY_EXISTS         = 0x0002,
	MEMPROTO_RES_VALUE_TOO_BIG      = 0x0003,
	MEMPROTO_RES_INVALID_ARGUMENTS  = 0x0004,
	MEMPROTO_RES_ITEM_NOT_STORED    = 0x0005,
	MEMPROTO_RES_UNKNOWN_COMMAND    = 0x0081,
	MEMPROTO_RES_OUT_OF_MEMORY      = 0x0082,
	MEMPROTO_RES_PAUSE              = 0xfe00,
	MEMPROTO_RES_IO_ERROR           = 0xff00,
} memproto_response_status;


typedef enum {
	MEMPROTO_CMD_GET                 = 0x00,
	MEMPROTO_CMD_SET                 = 0x01,
	MEMPROTO_CMD_ADD                 = 0x02,
	MEMPROTO_CMD_REPLACE             = 0x03,
	MEMPROTO_CMD_DELETE              = 0x04,
	MEMPROTO_CMD_INCREMENT           = 0x05,
	MEMPROTO_CMD_DECREMENT           = 0x06,
	MEMPROTO_CMD_QUIT                = 0x07,
	MEMPROTO_CMD_FLUSH               = 0x08,
	MEMPROTO_CMD_GETQ                = 0x09,
	MEMPROTO_CMD_NOOP                = 0x0a,
	MEMPROTO_CMD_VERSION             = 0x0b,
	MEMPROTO_CMD_GETK                = 0x0c,
	MEMPROTO_CMD_GETKQ               = 0x0d,
	MEMPROTO_CMD_APPEND              = 0x0e,
	MEMPROTO_CMD_PREPEND             = 0x0f,
} memproto_command;


typedef enum {
	MEMPROTO_TYPE_RAW_BYTES          = 0x00,
} memproto_datatype;


typedef struct memproto_header_ {
	uint8_t magic;
	uint8_t opcode;
	uint8_t data_type;
	uint16_t reserved;
	uint32_t opaque;
	uint64_t cas;
} memproto_header;


#define MEMPROTO_HEADER_SIZE 24


typedef struct memproto_callback_ {
	void (*cb_get      )(void* user, memproto_header* h,
			const char* key, uint16_t keylen);

	void (*cb_set      )(void* user, memproto_header* h,
			const char* key, uint16_t keylen,
			const char* val, uint32_t vallen,
			uint32_t flags, uint32_t expiration);

	void (*cb_add      )(void* user, memproto_header* h,
			const char* key, uint16_t keylen,
			const char* val, uint32_t vallen,
			uint32_t flags, uint32_t expiration);

	void (*cb_replace  )(void* user, memproto_header* h,
			const char* key, uint16_t keylen,
			const char* val, uint32_t vallen,
			uint32_t flags, uint32_t expiration);

	void (*cb_delete   )(void* user, memproto_header* h,
			const char* key, uint16_t keylen,
			uint32_t expiration);

	void (*cb_increment)(void* user, memproto_header* h,
			const char* key, uint16_t keylen,
			uint64_t amount, uint64_t initial, uint32_t expiration);

	void (*cb_decrement)(void* user, memproto_header* h,
			const char* key, uint16_t keylen,
			uint64_t amount, uint64_t initial, uint32_t expiration);

	void (*cb_quit     )(void* user, memproto_header* h);

	void (*cb_flush    )(void* user, memproto_header* h,
			uint32_t expiration);

	void (*cb_getq     )(void* user, memproto_header* h,
			const char* key, uint16_t keylen);

	void (*cb_noop     )(void* user, memproto_header* h);

	void (*cb_version  )(void* user, memproto_header* h);


	void (*cb_getk     )(void* user, memproto_header* h,
			const char* key, uint16_t keylen);

	void (*cb_getkq    )(void* user, memproto_header* h,
			const char* key, uint16_t keylen);

	void (*cb_append   )(void* user, memproto_header* h,
			const char* key, uint16_t keylen,
			const char* val, uint32_t vallen);

	void (*cb_prepend  )(void* user, memproto_header* h,
			const char* key, uint16_t keylen,
			const char* val, uint32_t vallen);

} memproto_callback;


typedef struct memproto_parser_ {
	const char* header;
	void* callback[16];
	void* user;
} memproto_parser;


/**
 * initialize parser context.
 * @param ctx   uninitialized parser context structure
 * @param cb    initialized callback structure. contents of it will be copied
 * @param user  this parameter will be passed to the callback function.
 */
void memproto_parser_init(memproto_parser* ctx, memproto_callback* cb, void* user);

/**
 * parse data.
 * @return 0 if parsing is not completed, >0 if parsing is completed, <0 if failed
 * @param ctx   initialized parser context structure
 * @param data  buffer. parsing range is from (buffer + off) up to (buffer + len - off)
 * @param len   size of buffer
 * @param off   this parameter will be changed
 */
int memproto_parser_execute(memproto_parser* ctx, const char* data, size_t len, size_t* off);

/**
 * dispatch.
 */
#define MEMPROTO_INVALID_ARGUMENT (-128)
int memproto_dispatch(memproto_parser* ctx);


#ifdef __cplusplus
}
#endif

#endif /* memproto.h */

