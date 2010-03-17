/*
 * memtext  memcached text protocol parser
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

#ifndef MEMTEXT_H__
#define MEMTEXT_H__

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define MEMTEXT_MAX_MULTI_GET 256

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	/* retrieval */
	MEMTEXT_CMD_GET,
	MEMTEXT_CMD_GETS,

	/* storage */
	MEMTEXT_CMD_SET,
	MEMTEXT_CMD_ADD,
	MEMTEXT_CMD_REPLACE,
	MEMTEXT_CMD_APPEND,
	MEMTEXT_CMD_PREPEND,

	/* cas */
	MEMTEXT_CMD_CAS,

	/* delete */
	MEMTEXT_CMD_DELETE,

	/* numeric */
	MEMTEXT_CMD_INCR,
	MEMTEXT_CMD_DECR,

	/* other */
	MEMTEXT_CMD_VERSION,
} memtext_command;


typedef struct {
	const char** key;
	unsigned* key_len;
	unsigned key_num;
} memtext_request_retrieval;

typedef struct {
	const char* key;
	unsigned key_len;
	const char* data;
	unsigned data_len;
	unsigned short flags;
	uint32_t exptime;
	bool noreply;
} memtext_request_storage;

typedef struct {
	const char* key;
	unsigned key_len;
	const char* data;
	unsigned data_len;
	unsigned short flags;
	uint32_t exptime;
	bool noreply;
	uint64_t cas_unique;
} memtext_request_cas;

typedef struct {
	const char* key;
	unsigned key_len;
	uint32_t exptime;
	bool noreply;
} memtext_request_delete;

typedef struct {
	const char* key;
	unsigned key_len;
	uint64_t value;
	bool noreply;
} memtext_request_numeric;

typedef struct {
} memtext_request_other;

typedef int (*memtext_callback_retrieval)(
		void* user, memtext_command cmd,
		memtext_request_retrieval* req);

typedef int (*memtext_callback_storage)(
		void* user, memtext_command cmd,
		memtext_request_storage* req);

typedef int (*memtext_callback_cas)(
		void* user, memtext_command cmd,
		memtext_request_cas* req);

typedef int (*memtext_callback_delete)(
		void* user, memtext_command cmd,
		memtext_request_delete* req);

typedef int (*memtext_callback_numeric)(
		void* user, memtext_command cmd,
		memtext_request_numeric* req);

typedef int (*memtext_callback_other)(
		void* user, memtext_command cmd,
		memtext_request_other* req);

typedef struct {
	memtext_callback_retrieval cmd_get;
	memtext_callback_retrieval cmd_gets;
	memtext_callback_storage   cmd_set;
	memtext_callback_storage   cmd_add;
	memtext_callback_storage   cmd_replace;
	memtext_callback_storage   cmd_append;
	memtext_callback_storage   cmd_prepend;
	memtext_callback_cas       cmd_cas;
	memtext_callback_delete    cmd_delete;
	memtext_callback_numeric   cmd_incr;
	memtext_callback_numeric   cmd_decr;
	memtext_callback_other     cmd_version;
} memtext_callback;

typedef struct {
	size_t data_count;

	int cs;
	int top;
	int stack[1];

	memtext_command command;

	size_t key_pos[MEMTEXT_MAX_MULTI_GET];
	unsigned int key_len[MEMTEXT_MAX_MULTI_GET];
	unsigned int keys;

	size_t flags;
	uint32_t exptime;
	size_t bytes;
	bool noreply;
	uint64_t cas_unique;

	size_t data_pos;
	unsigned int data_len;

	memtext_callback callback;

	void* user;
} memtext_parser;

void memtext_init(memtext_parser* ctx, memtext_callback* callback, void* user);
int memtext_execute(memtext_parser* ctx, const char* data, size_t len, size_t* off);

#ifdef __cplusplus
}
#endif

#endif /* memtext.h */

