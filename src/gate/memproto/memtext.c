#line 1 "memproto/memtext.rl"
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

#line 288 "memproto/memtext.rl"




#line 63 "memproto/memtext.c.tmp"
static const char _memtext_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	4, 1, 5, 1, 6, 1, 7, 1, 
	8, 1, 9, 1, 10, 1, 11, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	24, 1, 25, 1, 26, 1, 27, 1, 
	28, 1, 29, 1, 30, 2, 3, 1
	
};

static const unsigned char _memtext_key_offsets[] = {
	0, 0, 8, 10, 11, 12, 16, 20, 
	23, 24, 27, 28, 30, 34, 35, 36, 
	37, 38, 39, 40, 44, 48, 51, 52, 
	55, 56, 58, 61, 64, 66, 67, 68, 
	69, 70, 71, 72, 73, 74, 75, 76, 
	77, 81, 84, 87, 88, 90, 91, 92, 
	96, 100, 103, 105, 106, 107, 108, 109, 
	110, 111, 112, 113, 114, 118, 119, 120, 
	121, 122, 126, 130, 131, 135, 137, 138, 
	139, 140, 141, 142, 143, 144, 145, 149, 
	150, 151, 153, 157, 161, 162, 166, 167, 
	168, 169, 170, 171, 172, 173, 174, 175, 
	176, 177, 178, 179, 180, 181, 182, 183, 
	184, 186, 187, 188, 189, 190, 191, 192, 
	194, 195, 198, 201, 202, 203, 204, 205, 
	213
};

static const char _memtext_trans_keys[] = {
	97, 99, 100, 103, 105, 112, 114, 115, 
	100, 112, 100, 32, 0, 10, 13, 32, 
	0, 10, 13, 32, 48, 49, 57, 32, 
	48, 49, 57, 32, 49, 57, 13, 32, 
	48, 57, 10, 13, 10, 97, 115, 32, 
	0, 10, 13, 32, 0, 10, 13, 32, 
	48, 49, 57, 32, 48, 49, 57, 32, 
	49, 57, 32, 48, 57, 48, 49, 57, 
	13, 32, 10, 13, 10, 110, 111, 114, 
	101, 112, 108, 121, 13, 13, 32, 48, 
	57, 32, 48, 57, 32, 48, 57, 101, 
	99, 108, 114, 32, 0, 10, 13, 32, 
	0, 10, 13, 32, 48, 49, 57, 13, 
	32, 10, 110, 111, 114, 101, 112, 108, 
	121, 13, 13, 32, 48, 57, 101, 116, 
	101, 32, 0, 10, 13, 32, 0, 10, 
	13, 32, 10, 48, 110, 49, 57, 13, 
	32, 110, 111, 114, 101, 112, 108, 121, 
	13, 13, 32, 48, 57, 101, 116, 32, 
	115, 0, 10, 13, 32, 0, 10, 13, 
	32, 10, 0, 10, 13, 32, 32, 110, 
	99, 114, 114, 101, 112, 101, 110, 100, 
	101, 112, 108, 97, 99, 101, 101, 116, 
	13, 110, 111, 114, 101, 112, 108, 121, 
	13, 32, 13, 32, 48, 57, 32, 48, 
	57, 112, 101, 110, 100, 97, 99, 100, 
	103, 105, 112, 114, 115, 0
};

static const char _memtext_single_lengths[] = {
	0, 8, 2, 1, 1, 4, 4, 1, 
	1, 1, 1, 0, 2, 1, 1, 1, 
	1, 1, 1, 4, 4, 1, 1, 1, 
	1, 0, 1, 1, 2, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	2, 1, 1, 1, 2, 1, 1, 4, 
	4, 1, 2, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 2, 1, 1, 1, 
	1, 4, 4, 1, 2, 2, 1, 1, 
	1, 1, 1, 1, 1, 1, 2, 1, 
	1, 2, 4, 4, 1, 4, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	2, 1, 1, 1, 1, 1, 1, 2, 
	1, 1, 1, 1, 1, 1, 1, 8, 
	0
};

static const char _memtext_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 1, 
	0, 1, 0, 1, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 0, 1, 
	0, 1, 1, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 1, 1, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 1, 0, 0, 0, 0, 0, 
	0
};

static const short _memtext_index_offsets[] = {
	0, 0, 9, 12, 14, 16, 21, 26, 
	29, 31, 34, 36, 38, 42, 44, 46, 
	48, 50, 52, 54, 59, 64, 67, 69, 
	72, 74, 76, 79, 82, 85, 87, 89, 
	91, 93, 95, 97, 99, 101, 103, 105, 
	107, 111, 114, 117, 119, 122, 124, 126, 
	131, 136, 139, 142, 144, 146, 148, 150, 
	152, 154, 156, 158, 160, 164, 166, 168, 
	170, 172, 177, 182, 184, 188, 191, 193, 
	195, 197, 199, 201, 203, 205, 207, 211, 
	213, 215, 218, 223, 228, 230, 235, 237, 
	239, 241, 243, 245, 247, 249, 251, 253, 
	255, 257, 259, 261, 263, 265, 267, 269, 
	271, 274, 276, 278, 280, 282, 284, 286, 
	289, 291, 294, 297, 299, 301, 303, 305, 
	314
};

static const unsigned char _memtext_indicies[] = {
	0, 2, 3, 4, 5, 6, 7, 8, 
	1, 9, 10, 1, 11, 1, 12, 1, 
	1, 1, 1, 1, 13, 1, 1, 1, 
	15, 14, 16, 17, 1, 18, 1, 19, 
	20, 1, 21, 1, 22, 1, 23, 24, 
	25, 1, 26, 1, 27, 1, 28, 1, 
	29, 1, 30, 1, 31, 1, 1, 1, 
	1, 1, 32, 1, 1, 1, 34, 33, 
	35, 36, 1, 37, 1, 38, 39, 1, 
	40, 1, 41, 1, 42, 43, 1, 44, 
	45, 1, 46, 47, 1, 48, 1, 49, 
	1, 50, 1, 51, 1, 52, 1, 53, 
	1, 54, 1, 55, 1, 56, 1, 57, 
	1, 58, 1, 46, 47, 59, 1, 40, 
	60, 1, 37, 61, 1, 62, 1, 63, 
	64, 1, 65, 1, 66, 1, 1, 1, 
	1, 1, 67, 1, 1, 1, 69, 68, 
	70, 71, 1, 72, 73, 1, 74, 1, 
	75, 1, 76, 1, 77, 1, 78, 1, 
	79, 1, 80, 1, 81, 1, 82, 1, 
	72, 73, 83, 1, 84, 1, 85, 1, 
	86, 1, 87, 1, 1, 1, 1, 1, 
	88, 1, 1, 90, 91, 89, 92, 1, 
	93, 95, 94, 1, 96, 97, 1, 95, 
	1, 98, 1, 99, 1, 100, 1, 101, 
	1, 102, 1, 103, 1, 104, 1, 96, 
	97, 105, 1, 106, 1, 107, 1, 108, 
	109, 1, 1, 1, 1, 1, 110, 1, 
	1, 112, 113, 111, 114, 1, 1, 1, 
	116, 1, 115, 108, 1, 117, 1, 118, 
	1, 119, 1, 120, 1, 121, 1, 122, 
	1, 123, 1, 124, 1, 125, 1, 126, 
	1, 127, 1, 128, 1, 129, 1, 130, 
	1, 131, 1, 132, 1, 133, 1, 134, 
	135, 1, 136, 1, 137, 1, 138, 1, 
	139, 1, 140, 1, 141, 1, 142, 143, 
	1, 134, 1, 21, 144, 1, 18, 145, 
	1, 146, 1, 147, 1, 148, 1, 149, 
	1, 0, 2, 3, 4, 5, 6, 7, 
	8, 1, 150, 0
};

static const char _memtext_trans_targs[] = {
	2, 0, 16, 43, 79, 87, 90, 96, 
	102, 3, 115, 4, 5, 6, 6, 7, 
	8, 114, 9, 10, 113, 11, 12, 13, 
	104, 12, 14, 15, 119, 17, 18, 19, 
	20, 20, 21, 22, 42, 23, 24, 41, 
	25, 26, 27, 26, 28, 40, 29, 32, 
	30, 31, 119, 33, 34, 35, 36, 37, 
	38, 39, 29, 40, 41, 42, 44, 45, 
	61, 46, 47, 48, 48, 49, 50, 60, 
	51, 52, 119, 53, 54, 55, 56, 57, 
	58, 59, 51, 60, 62, 63, 64, 65, 
	66, 66, 67, 68, 119, 69, 78, 71, 
	67, 70, 72, 73, 74, 75, 76, 77, 
	67, 78, 80, 81, 82, 86, 83, 83, 
	84, 85, 119, 83, 84, 88, 89, 46, 
	91, 92, 93, 94, 95, 4, 97, 98, 
	99, 100, 101, 4, 103, 4, 13, 105, 
	106, 107, 108, 109, 110, 111, 13, 112, 
	113, 114, 116, 117, 118, 4, 120
};

static const char _memtext_trans_actions[] = {
	1, 0, 1, 1, 1, 1, 1, 1, 
	1, 0, 0, 35, 0, 3, 0, 5, 
	7, 7, 9, 11, 11, 13, 15, 17, 
	17, 0, 25, 0, 53, 0, 43, 0, 
	3, 0, 5, 7, 7, 9, 11, 11, 
	13, 15, 17, 0, 21, 21, 23, 23, 
	25, 0, 55, 0, 0, 0, 0, 0, 
	0, 0, 19, 0, 0, 0, 0, 0, 
	0, 49, 0, 3, 0, 5, 21, 21, 
	23, 23, 59, 0, 0, 0, 0, 0, 
	0, 0, 19, 0, 0, 0, 45, 0, 
	3, 0, 5, 5, 57, 11, 11, 0, 
	13, 13, 0, 0, 0, 0, 0, 0, 
	19, 0, 0, 29, 0, 31, 3, 0, 
	5, 5, 51, 61, 0, 0, 0, 47, 
	0, 0, 0, 0, 0, 41, 0, 0, 
	0, 0, 0, 37, 0, 33, 0, 0, 
	0, 0, 0, 0, 0, 0, 19, 19, 
	0, 0, 0, 0, 0, 39, 27
};

static const int memtext_start = 1;
static const int memtext_first_final = 119;
static const int memtext_error = 0;

static const int memtext_en_main = 1;
static const int memtext_en_data = 120;

#line 292 "memproto/memtext.rl"

void memtext_init(memtext_parser* ctx, memtext_callback* callback, void* user)
{
	int cs = 0;
	int top = 0;
	
#line 283 "memproto/memtext.c.tmp"
	{
	cs = memtext_start;
	top = 0;
	}
#line 298 "memproto/memtext.rl"
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

	
#line 318 "memproto/memtext.c.tmp"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _memtext_trans_keys + _memtext_key_offsets[cs];
	_trans = _memtext_index_offsets[cs];

	_klen = _memtext_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _memtext_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += ((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _memtext_indicies[_trans];
	cs = _memtext_trans_targs[_trans];

	if ( _memtext_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _memtext_actions + _memtext_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 59 "memproto/memtext.rl"
	{
		ctx->keys = 0;
		ctx->noreply = false;
		ctx->exptime = 0;
	}
	break;
	case 1:
#line 65 "memproto/memtext.rl"
	{
		MARK(key_pos[ctx->keys], p);
	}
	break;
	case 2:
#line 68 "memproto/memtext.rl"
	{
		SET_MARK_LEN(key_len[ctx->keys], key_pos[ctx->keys], p);
	}
	break;
	case 3:
#line 71 "memproto/memtext.rl"
	{
		++ctx->keys;
		if(ctx->keys > MEMTEXT_MAX_MULTI_GET) {
			goto convert_error;
		}
	}
	break;
	case 4:
#line 78 "memproto/memtext.rl"
	{
		MARK(flags, p);
	}
	break;
	case 5:
#line 81 "memproto/memtext.rl"
	{
		SET_UINT(flags, flags, p);
	}
	break;
	case 6:
#line 85 "memproto/memtext.rl"
	{
		MARK(exptime, p);
	}
	break;
	case 7:
#line 88 "memproto/memtext.rl"
	{
		SET_UINT(exptime, exptime, p);
	}
	break;
	case 8:
#line 92 "memproto/memtext.rl"
	{
		MARK(bytes, p);
	}
	break;
	case 9:
#line 95 "memproto/memtext.rl"
	{
		SET_UINT(bytes, bytes, p);
	}
	break;
	case 10:
#line 99 "memproto/memtext.rl"
	{
		ctx->noreply = true;
	}
	break;
	case 11:
#line 103 "memproto/memtext.rl"
	{
		MARK(cas_unique, p);
	}
	break;
	case 12:
#line 106 "memproto/memtext.rl"
	{
		SET_ULL(cas_unique, cas_unique, p);
	}
	break;
	case 13:
#line 110 "memproto/memtext.rl"
	{
		MARK(data_pos, p+1);
		ctx->data_count = ctx->bytes;
		{stack[top++] = cs; cs = 120; goto _again;}
	}
	break;
	case 14:
#line 115 "memproto/memtext.rl"
	{
		if(--ctx->data_count == 0) {
			//printf("mark %d\n", ctx->data_pos);
			//printf("fpc %p\n", fpc);
			//printf("data %p\n", data);
			SET_MARK_LEN(data_len, data_pos, p+1);
			{cs = stack[--top]; goto _again;}
		}
	}
	break;
	case 15:
#line 126 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_GET;     }
	break;
	case 16:
#line 127 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_GETS;    }
	break;
	case 17:
#line 128 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_SET;     }
	break;
	case 18:
#line 129 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_ADD;     }
	break;
	case 19:
#line 130 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_REPLACE; }
	break;
	case 20:
#line 131 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_APPEND;  }
	break;
	case 21:
#line 132 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_PREPEND; }
	break;
	case 22:
#line 133 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_CAS;     }
	break;
	case 23:
#line 134 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_DELETE;  }
	break;
	case 24:
#line 135 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_INCR;    }
	break;
	case 25:
#line 136 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_DECR;    }
	break;
	case 26:
#line 139 "memproto/memtext.rl"
	{
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
	break;
	case 27:
#line 158 "memproto/memtext.rl"
	{
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
	break;
	case 28:
#line 174 "memproto/memtext.rl"
	{
		CALLBACK(cb, memtext_callback_cas);
		if(cb) {
			memtext_request_cas req = {
				MARK_PTR(key_pos[0]), ctx->key_len[0],
				MARK_PTR(data_pos), ctx->data_len,
				ctx->flags,
				ctx->exptime,
				ctx->cas_unique,
				ctx->noreply
			};
			if((*cb)(ctx->user, ctx->command, &req) < 0) {
				goto convert_error;
			}
		} else { goto convert_error; }
	}
	break;
	case 29:
#line 191 "memproto/memtext.rl"
	{
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
	break;
	case 30:
#line 204 "memproto/memtext.rl"
	{
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
	break;
#line 627 "memproto/memtext.c.tmp"
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	_out: {}
	}
#line 327 "memproto/memtext.rl"

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

