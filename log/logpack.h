/*
 * MessagePack fast log format
 *
 * Copyright (C) 2008-2009 FURUHASHI Sadayuki
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
#ifndef LOGPACK_H__
#define LOGPACK_H__

#include <stddef.h>
#include <msgpack.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct logpack_t logpack_t;

logpack_t* logpack_new(const char* basename, size_t lotate_size);
void logpack_free(logpack_t* lpk);

int logpack_write_raw(logpack_t* lpk, const char* buf, size_t size);


#if 0
typedef struct {
	msgpack_pack_t packer;
	msgpack_sbuffer buffer;
} logpack_log_t;

int logpack_log_init(logpack_log_t* pac, const char* name, uint16_t version);
void logpack_log_destroy(logpack_log_t* pac);
int logpack_write(logpack_t* lpk, const logpack_log_t* pac);
#endif


#ifdef __cplusplus
}
#endif

#endif /* logpack.h */

