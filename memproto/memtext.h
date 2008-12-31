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

typedef int (*memtext_callback_retrieval)(
		void* user,
		const char** key, unsigned* key_len, unsigned keys);

typedef int (*memtext_callback_storage)(
		void* user,
		const char* key, unsigned key_len,
		unsigned short flags, uint32_t exptime,
		const char* data, unsigned data_len,
		bool noreply);

typedef int (*memtext_callback_cas)(
		void* user,
		const char* key, unsigned key_len,
		unsigned short flags, uint32_t exptime,
		const char* data, unsigned data_len,
		uint64_t cas_unique,
		bool noreply);

typedef int (*memtext_callback_delete)(
		void* user,
		const char* key, unsigned key_len,
		uint32_t exptime, bool noreply);

typedef struct {
	memtext_callback_retrieval cmd_get;
	memtext_callback_storage   cmd_set;
	memtext_callback_storage   cmd_replace;
	memtext_callback_storage   cmd_append;
	memtext_callback_storage   cmd_prepend;
	memtext_callback_cas       cmd_cas;
	memtext_callback_delete    cmd_delete;
} memtext_callback;

typedef struct {
	size_t data_count;

	int cs;
	int top;
	int stack[1];

	int command;

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

