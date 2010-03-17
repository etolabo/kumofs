
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


#line 306 "memproto/memtext.rl"




#line 65 "memproto/memtext.c.tmp"
static const char _memtext_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	4, 1, 5, 1, 6, 1, 7, 1, 
	8, 1, 9, 1, 10, 1, 11, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	24, 1, 25, 1, 26, 1, 27, 1, 
	28, 1, 29, 1, 30, 1, 31, 1, 
	32, 2, 3, 1
};

static const unsigned char _memtext_key_offsets[] = {
	0, 0, 9, 11, 12, 13, 17, 21, 
	24, 25, 28, 29, 31, 35, 36, 37, 
	38, 39, 40, 41, 45, 49, 52, 53, 
	56, 57, 59, 62, 65, 67, 68, 69, 
	70, 71, 72, 73, 74, 75, 76, 77, 
	78, 82, 85, 88, 89, 91, 92, 93, 
	97, 101, 104, 106, 107, 108, 109, 110, 
	111, 112, 113, 114, 115, 119, 120, 121, 
	122, 123, 127, 131, 132, 136, 138, 139, 
	140, 141, 142, 143, 144, 145, 146, 150, 
	151, 152, 154, 158, 162, 163, 167, 168, 
	169, 170, 171, 172, 173, 174, 175, 176, 
	177, 178, 179, 180, 181, 182, 183, 184, 
	185, 186, 187, 188, 189, 190, 191, 192, 
	193, 195, 196, 197, 198, 199, 200, 201, 
	203, 204, 207, 210, 211, 212, 213, 214, 
	223
};

static const char _memtext_trans_keys[] = {
	97, 99, 100, 103, 105, 112, 114, 115, 
	118, 100, 112, 100, 32, 0, 10, 13, 
	32, 0, 10, 13, 32, 48, 49, 57, 
	32, 48, 49, 57, 32, 49, 57, 13, 
	32, 48, 57, 10, 13, 10, 97, 115, 
	32, 0, 10, 13, 32, 0, 10, 13, 
	32, 48, 49, 57, 32, 48, 49, 57, 
	32, 49, 57, 32, 48, 57, 48, 49, 
	57, 13, 32, 10, 13, 10, 110, 111, 
	114, 101, 112, 108, 121, 13, 13, 32, 
	48, 57, 32, 48, 57, 32, 48, 57, 
	101, 99, 108, 114, 32, 0, 10, 13, 
	32, 0, 10, 13, 32, 48, 49, 57, 
	13, 32, 10, 110, 111, 114, 101, 112, 
	108, 121, 13, 13, 32, 48, 57, 101, 
	116, 101, 32, 0, 10, 13, 32, 0, 
	10, 13, 32, 10, 48, 110, 49, 57, 
	13, 32, 110, 111, 114, 101, 112, 108, 
	121, 13, 13, 32, 48, 57, 101, 116, 
	32, 115, 0, 10, 13, 32, 0, 10, 
	13, 32, 10, 0, 10, 13, 32, 32, 
	110, 99, 114, 114, 101, 112, 101, 110, 
	100, 101, 112, 108, 97, 99, 101, 101, 
	116, 101, 114, 115, 105, 111, 110, 13, 
	10, 13, 110, 111, 114, 101, 112, 108, 
	121, 13, 32, 13, 32, 48, 57, 32, 
	48, 57, 112, 101, 110, 100, 97, 99, 
	100, 103, 105, 112, 114, 115, 118, 0
};

static const char _memtext_single_lengths[] = {
	0, 9, 2, 1, 1, 4, 4, 1, 
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
	1, 1, 1, 1, 1, 1, 1, 1, 
	2, 1, 1, 1, 1, 1, 1, 2, 
	1, 1, 1, 1, 1, 1, 1, 9, 
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
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 1, 0, 0, 0, 0, 0, 
	0
};

static const short _memtext_index_offsets[] = {
	0, 0, 10, 13, 15, 17, 22, 27, 
	30, 32, 35, 37, 39, 43, 45, 47, 
	49, 51, 53, 55, 60, 65, 68, 70, 
	73, 75, 77, 80, 83, 86, 88, 90, 
	92, 94, 96, 98, 100, 102, 104, 106, 
	108, 112, 115, 118, 120, 123, 125, 127, 
	132, 137, 140, 143, 145, 147, 149, 151, 
	153, 155, 157, 159, 161, 165, 167, 169, 
	171, 173, 178, 183, 185, 189, 192, 194, 
	196, 198, 200, 202, 204, 206, 208, 212, 
	214, 216, 219, 224, 229, 231, 236, 238, 
	240, 242, 244, 246, 248, 250, 252, 254, 
	256, 258, 260, 262, 264, 266, 268, 270, 
	272, 274, 276, 278, 280, 282, 284, 286, 
	288, 291, 293, 295, 297, 299, 301, 303, 
	306, 308, 311, 314, 316, 318, 320, 322, 
	332
};

static const unsigned char _memtext_indicies[] = {
	0, 2, 3, 4, 5, 6, 7, 8, 
	9, 1, 10, 11, 1, 12, 1, 13, 
	1, 1, 1, 1, 1, 14, 1, 1, 
	1, 16, 15, 17, 18, 1, 19, 1, 
	20, 21, 1, 22, 1, 23, 1, 24, 
	25, 26, 1, 27, 1, 28, 1, 29, 
	1, 30, 1, 31, 1, 32, 1, 1, 
	1, 1, 1, 33, 1, 1, 1, 35, 
	34, 36, 37, 1, 38, 1, 39, 40, 
	1, 41, 1, 42, 1, 43, 44, 1, 
	45, 46, 1, 47, 48, 1, 49, 1, 
	50, 1, 51, 1, 52, 1, 53, 1, 
	54, 1, 55, 1, 56, 1, 57, 1, 
	58, 1, 59, 1, 47, 48, 60, 1, 
	41, 61, 1, 38, 62, 1, 63, 1, 
	64, 65, 1, 66, 1, 67, 1, 1, 
	1, 1, 1, 68, 1, 1, 1, 70, 
	69, 71, 72, 1, 73, 74, 1, 75, 
	1, 76, 1, 77, 1, 78, 1, 79, 
	1, 80, 1, 81, 1, 82, 1, 83, 
	1, 73, 74, 84, 1, 85, 1, 86, 
	1, 87, 1, 88, 1, 1, 1, 1, 
	1, 89, 1, 1, 91, 92, 90, 93, 
	1, 94, 96, 95, 1, 97, 98, 1, 
	96, 1, 99, 1, 100, 1, 101, 1, 
	102, 1, 103, 1, 104, 1, 105, 1, 
	97, 98, 106, 1, 107, 1, 108, 1, 
	109, 110, 1, 1, 1, 1, 1, 111, 
	1, 1, 113, 114, 112, 115, 1, 1, 
	1, 117, 1, 116, 109, 1, 118, 1, 
	119, 1, 120, 1, 121, 1, 122, 1, 
	123, 1, 124, 1, 125, 1, 126, 1, 
	127, 1, 128, 1, 129, 1, 130, 1, 
	131, 1, 132, 1, 133, 1, 134, 1, 
	135, 1, 136, 1, 137, 1, 138, 1, 
	139, 1, 140, 1, 141, 1, 142, 1, 
	143, 144, 1, 145, 1, 146, 1, 147, 
	1, 148, 1, 149, 1, 150, 1, 151, 
	152, 1, 143, 1, 22, 153, 1, 19, 
	154, 1, 155, 1, 156, 1, 157, 1, 
	158, 1, 0, 2, 3, 4, 5, 6, 
	7, 8, 9, 1, 159, 0
};

static const unsigned char _memtext_trans_targs[] = {
	2, 0, 16, 43, 79, 87, 90, 96, 
	102, 104, 3, 123, 4, 5, 6, 6, 
	7, 8, 122, 9, 10, 121, 11, 12, 
	13, 112, 12, 14, 15, 127, 17, 18, 
	19, 20, 20, 21, 22, 42, 23, 24, 
	41, 25, 26, 27, 26, 28, 40, 29, 
	32, 30, 31, 127, 33, 34, 35, 36, 
	37, 38, 39, 29, 40, 41, 42, 44, 
	45, 61, 46, 47, 48, 48, 49, 50, 
	60, 51, 52, 127, 53, 54, 55, 56, 
	57, 58, 59, 51, 60, 62, 63, 64, 
	65, 66, 66, 67, 68, 127, 69, 78, 
	71, 67, 70, 72, 73, 74, 75, 76, 
	77, 67, 78, 80, 81, 82, 86, 83, 
	83, 84, 85, 127, 83, 84, 88, 89, 
	46, 91, 92, 93, 94, 95, 4, 97, 
	98, 99, 100, 101, 4, 103, 4, 105, 
	106, 107, 108, 109, 110, 111, 127, 13, 
	113, 114, 115, 116, 117, 118, 119, 13, 
	120, 121, 122, 124, 125, 126, 4, 128
};

static const char _memtext_trans_actions[] = {
	1, 0, 1, 1, 1, 1, 1, 1, 
	1, 1, 0, 0, 35, 0, 3, 0, 
	5, 7, 7, 9, 11, 11, 13, 15, 
	17, 17, 0, 25, 0, 55, 0, 43, 
	0, 3, 0, 5, 7, 7, 9, 11, 
	11, 13, 15, 17, 0, 21, 21, 23, 
	23, 25, 0, 57, 0, 0, 0, 0, 
	0, 0, 0, 19, 0, 0, 0, 0, 
	0, 0, 49, 0, 3, 0, 5, 21, 
	21, 23, 23, 61, 0, 0, 0, 0, 
	0, 0, 0, 19, 0, 0, 0, 45, 
	0, 3, 0, 5, 5, 59, 11, 11, 
	0, 13, 13, 0, 0, 0, 0, 0, 
	0, 19, 0, 0, 29, 0, 31, 3, 
	0, 5, 5, 53, 65, 0, 0, 0, 
	47, 0, 0, 0, 0, 0, 41, 0, 
	0, 0, 0, 0, 37, 0, 33, 0, 
	0, 0, 0, 0, 51, 0, 63, 0, 
	0, 0, 0, 0, 0, 0, 0, 19, 
	19, 0, 0, 0, 0, 0, 39, 27
};

static const int memtext_start = 1;
static const int memtext_first_final = 127;
static const int memtext_error = 0;

static const int memtext_en_main = 1;
static const int memtext_en_data = 128;


#line 310 "memproto/memtext.rl"

void memtext_init(memtext_parser* ctx, memtext_callback* callback, void* user)
{
	int cs = 0;
	int top = 0;
	
#line 295 "memproto/memtext.c.tmp"
	{
	cs = memtext_start;
	top = 0;
	}

#line 316 "memproto/memtext.rl"
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

	
#line 331 "memproto/memtext.c.tmp"
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
		{stack[top++] = cs; cs = 128; goto _again;}
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
#line 137 "memproto/memtext.rl"
	{ ctx->command = MEMTEXT_CMD_VERSION; }
	break;
	case 27:
#line 140 "memproto/memtext.rl"
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
	case 28:
#line 159 "memproto/memtext.rl"
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
	case 29:
#line 175 "memproto/memtext.rl"
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
	case 30:
#line 192 "memproto/memtext.rl"
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
	case 31:
#line 205 "memproto/memtext.rl"
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
	case 32:
#line 218 "memproto/memtext.rl"
	{
		CALLBACK(cb, memtext_callback_other);
		if(cb) {
			memtext_request_other req;
			if((*cb)(ctx->user, ctx->command, &req) < 0) {
				goto convert_error;
			}
		} else { goto convert_error; }
	}
	break;
#line 656 "memproto/memtext.c.tmp"
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

#line 345 "memproto/memtext.rl"

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

