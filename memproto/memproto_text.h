/*
 * memproto  memcached text protocol parser
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

#ifndef MEMPROTO_TEXT_H__
#define MEMPROTO_TEXT_H__

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define MEMPROTO_TEXT_MAX_MULTI_GET 256

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*memproto_text_callback_retrieval)(
		void* user,
		const char** key, unsigned* key_len, unsigned keys);

typedef int (*memproto_text_callback_storage)(
		void* user,
		const char* key, unsigned key_len,
		unsigned short flags, uint64_t exptime,
		const char* data, unsigned data_len,
		bool noreply);

typedef int (*memproto_text_callback_cas)(
		void* user,
		const char* key, unsigned key_len,
		unsigned short flags, uint64_t exptime,
		const char* data, unsigned data_len,
		uint64_t cas_unique,
		bool noreply);

typedef int (*memproto_text_callback_delete)(
		void* user,
		const char* key, unsigned key_len,
		uint64_t time, bool noreply);

typedef struct {
	memproto_text_callback_retrieval cmd_get;
	memproto_text_callback_storage   cmd_set;
	memproto_text_callback_storage   cmd_replace;
	memproto_text_callback_storage   cmd_append;
	memproto_text_callback_storage   cmd_prepend;
	memproto_text_callback_cas       cmd_cas;
	memproto_text_callback_delete    cmd_delete;
} memproto_text_callback;

typedef struct {
	size_t data_count;

	int cs;
	int top;
	int stack[1];

	int command;

	size_t key_pos[MEMPROTO_TEXT_MAX_MULTI_GET];
	unsigned int key_len[MEMPROTO_TEXT_MAX_MULTI_GET];
	unsigned int keys;

	size_t flags;
	uint64_t exptime;
	size_t bytes;
	bool noreply;
	uint64_t time;
	uint64_t cas_unique;

	size_t data_pos;
	unsigned int data_len;

	memproto_text_callback callback;

	void* user;
} memproto_text;

void memproto_text_init(memproto_text* ctx, memproto_text_callback* callback, void* user);
int memproto_text_execute(memproto_text* ctx, const char* data, size_t len, size_t* off);

#ifdef __cplusplus
}
#endif

#endif /* memproto_text.h */

