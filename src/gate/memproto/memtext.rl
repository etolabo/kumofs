/*
 * memtext
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

#include "memtext.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MARK(M, FPC) (ctx->M = FPC - data)
#define MARK_LEN(M, FPC) (FPC - (ctx->M + data))
#define MARK_PTR(M) (ctx->M + data)

#define NUM_BUF_MAX 20

#define SET_INTEGER(DST, M, FPC, STRFUNC) \
	do { \
		pos = MARK_PTR(M); \
		if(pos[0] == '0') { ctx->DST = 0; } \
		else { \
			len = MARK_LEN(M, FPC); \
			if(len > NUM_BUF_MAX) { goto convert_error; } \
			memcpy(numbuf, pos, len); \
			numbuf[len] = '\0'; \
			ctx->DST = STRFUNC(numbuf, NULL, 10); \
			if(ctx->DST == 0) { goto convert_error; } \
		} \
	} while(0)

#define SET_UINT(DST, M, FPC) \
	SET_INTEGER(DST, M, FPC, strtoul)

#define SET_ULL(DST, M, FPC) \
	SET_INTEGER(DST, M, FPC, strtoull)

#define SET_MARK_LEN(DST, M, FPC) \
		ctx->DST = MARK_LEN(M, FPC);

#define CALLBACK(NAME, TYPE) \
	TYPE NAME = ((TYPE*)(&ctx->callback))[ctx->command]

%%{
	machine memtext;

	action reset {
		ctx->keys = 0;
		ctx->noreply = false;
		ctx->exptime = 0;
	}

	action mark_key {
		MARK(key_pos[ctx->keys], fpc);
	}
	action key {
		SET_MARK_LEN(key_len[ctx->keys], key_pos[ctx->keys], fpc);
	}
	action incr_key {
		++ctx->keys;
		if(ctx->keys > MEMTEXT_MAX_MULTI_GET) {
			goto convert_error;
		}
	}

	action mark_flags {
		MARK(flags, fpc);
	}
	action flags {
		SET_UINT(flags, flags, fpc);
	}

	action mark_exptime {
		MARK(exptime, fpc);
	}
	action exptime {
		SET_UINT(exptime, exptime, fpc);
	}

	action mark_bytes {
		MARK(bytes, fpc);
	}
	action bytes {
		SET_UINT(bytes, bytes, fpc);
	}

	action noreply {
		ctx->noreply = true;
	}

	action mark_cas_unique {
		MARK(cas_unique, fpc);
	}
	action cas_unique {
		SET_ULL(cas_unique, cas_unique, fpc);
	}

	action data_start {
		MARK(data_pos, fpc+1);
		ctx->data_count = ctx->bytes;
		fcall data;
	}
	action data {
		if(--ctx->data_count == 0) {
			//printf("mark %d\n", ctx->data_pos);
			//printf("fpc %p\n", fpc);
			//printf("data %p\n", data);
			SET_MARK_LEN(data_len, data_pos, fpc+1);
			fret;
		}
	}


	action cmd_get     { ctx->command = MEMTEXT_CMD_GET;     }
	action cmd_gets    { ctx->command = MEMTEXT_CMD_GETS;    }
	action cmd_set     { ctx->command = MEMTEXT_CMD_SET;     }
	action cmd_add     { ctx->command = MEMTEXT_CMD_ADD;     }
	action cmd_replace { ctx->command = MEMTEXT_CMD_REPLACE; }
	action cmd_append  { ctx->command = MEMTEXT_CMD_APPEND;  }
	action cmd_prepend { ctx->command = MEMTEXT_CMD_PREPEND; }
	action cmd_cas     { ctx->command = MEMTEXT_CMD_CAS;     }
	action cmd_delete  { ctx->command = MEMTEXT_CMD_DELETE;  }
	action cmd_incr    { ctx->command = MEMTEXT_CMD_INCR;    }
	action cmd_decr    { ctx->command = MEMTEXT_CMD_DECR;    }
	action cmd_version { ctx->command = MEMTEXT_CMD_VERSION; }


	action do_retrieval {
		unsigned int i;
		++ctx->keys;
		for(i=0; i < ctx->keys; ++i) {
			ctx->key_pos[i] = (size_t)MARK_PTR(key_pos[i]);
		}
		CALLBACK(cb, memtext_callback_retrieval);
		if(cb) {
			memtext_request_retrieval req = {
				(const char**)ctx->key_pos,
				ctx->key_len,
				ctx->keys
			};
			if((*cb)(ctx->user, ctx->command, &req) < 0) {
				goto convert_error;
			}
		} else { goto convert_error; }
	}

	action do_storage {
		CALLBACK(cb, memtext_callback_storage);
		if(cb) {
			memtext_request_storage req = {
				MARK_PTR(key_pos[0]), ctx->key_len[0],
				MARK_PTR(data_pos), ctx->data_len,
				ctx->flags,
				ctx->exptime,
				ctx->noreply
			};
			if((*cb)(ctx->user, ctx->command, &req) < 0) {
				goto convert_error;
			}
		} else { goto convert_error; }
	}

	action do_cas {
		CALLBACK(cb, memtext_callback_cas);
		if(cb) {
			memtext_request_cas req = {
				MARK_PTR(key_pos[0]), ctx->key_len[0],
				MARK_PTR(data_pos), ctx->data_len,
				ctx->flags,
				ctx->exptime,
				ctx->noreply,
				ctx->cas_unique
			};
			if((*cb)(ctx->user, ctx->command, &req) < 0) {
				goto convert_error;
			}
		} else { goto convert_error; }
	}

	action do_delete {
		CALLBACK(cb, memtext_callback_delete);
		if(cb) {
			memtext_request_delete req = {
				MARK_PTR(key_pos[0]), ctx->key_len[0],
				ctx->exptime, ctx->noreply
			};
			if((*cb)(ctx->user, ctx->command, &req) < 0) {
				goto convert_error;
			}
		} else { goto convert_error; }
	}

	action do_numeric {
		CALLBACK(cb, memtext_callback_numeric);
		if(cb) {
			memtext_request_numeric req = {
				MARK_PTR(key_pos[0]), ctx->key_len[0],
				ctx->cas_unique, ctx->noreply
			};
			if((*cb)(ctx->user, ctx->command, &req) < 0) {
				goto convert_error;
			}
		} else { goto convert_error; }
	}

	action do_other {
		CALLBACK(cb, memtext_callback_other);
		if(cb) {
			memtext_request_other req;
			if((*cb)(ctx->user, ctx->command, &req) < 0) {
				goto convert_error;
			}
		} else { goto convert_error; }
	}

	key        = ([^\r \0\n]+)       >mark_key        %key;
	#key       = ([\!-\~]+)          >mark_key        %key;
	flags      = ('0' | [1-9][0-9]*) >mark_flags      %flags;
	exptime    = ('0' | [1-9][0-9]*) >mark_exptime    %exptime;
	bytes      = ([1-9][0-9]*)       >mark_bytes      %bytes;
	noreply    = ('noreply')         %noreply;
	cas_unique = ('0' | [1-9][0-9]*) >mark_cas_unique %cas_unique;


	retrieval_command = ('gets') @cmd_gets
					  | ('get' ) @cmd_get
					  ;

	storage_command = ('set'     ) @cmd_set
					| ('add'     ) @cmd_add
					| ('replace' ) @cmd_replace
					| ('append'  ) @cmd_append
					| ('prepend' ) @cmd_prepend
					;

	cas_command = ('cas') @cmd_cas;

	delete_command = ('delete') @cmd_delete;

	numeric_command = ('incr') @cmd_incr
					| ('decr') @cmd_decr
					;

	other_command = ('version') @cmd_version;

	retrieval = retrieval_command ' ' key (' ' key >incr_key)*
				' '*   # workaraound for libmemcached
				'\r\n';

	storage = storage_command ' ' key
				' ' flags ' ' exptime ' ' bytes
				(' ' noreply)?
				' '*   # workaraound for apr_memcache and memcached client for java
				'\r\n'
				@data_start
				'\r\n'
				;

	cas = cas_command ' ' key
				' ' flags ' ' exptime ' ' bytes
				' ' cas_unique
				(' ' noreply)?
				' '*   # workaraound for memcached client for java
				'\r\n'
				@data_start
				'\r\n'
				;

	delete = delete_command ' ' key
				(' ' exptime)? (' ' noreply)?
				'\r\n'
				;

	numeric = numeric_command ' ' key
				' ' cas_unique  # cas_unique => value
				(' ' noreply)?
				'\r\n'
				;

	other = other_command
			'\r\n'
			;

	command = retrieval @do_retrieval
			| storage   @do_storage
			| cas       @do_cas
			| delete    @do_delete
			| numeric   @do_numeric
			| other     @do_other
			;

main := (command >reset)+;

data := (any @data)*;
}%%


%% write data;

void memtext_init(memtext_parser* ctx, memtext_callback* callback, void* user)
{
	int cs = 0;
	int top = 0;
	%% write init;
	memset(ctx, 0, sizeof(memtext_parser));
	ctx->cs = cs;
	ctx->callback = *callback;
	ctx->user = user;
}

int memtext_execute(memtext_parser* ctx, const char* data, size_t len, size_t* off)
{
	if(len <= *off) { return 0; }

	const char* p = data + *off;
	const char* pe = data + len;
	const char* eof = pe;
	int cs = ctx->cs;
	int top = ctx->top;
	int* stack = ctx->stack;
	const char* pos;
	char numbuf[NUM_BUF_MAX+1];

	//printf("execute, len:%lu, off:%lu\n", len, *off);
	//printf("%s\n", data);
	//printf("data: ");
	//int i;
	//for(i=0; i < len; ++i) {
	//	printf("0x%x ", (int)data[i]);
	//}
	//printf("\n");

	%% write exec;

ret:
	ctx->cs = cs;
	ctx->top = top;
	*off = p - data;

	if(cs == memtext_error) {
		return -1;
	} else if(cs == memtext_first_final) {
		return 1;
	} else {
		return 0;
	}

convert_error:
	cs = memtext_error;
	goto ret;
}

