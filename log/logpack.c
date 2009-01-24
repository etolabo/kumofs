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
#include "logpack.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#ifdef LOGPACK_ENABLE_PSHARED
typedef struct {
	int mapfd;
	volatile int seqnum;
} logpack_pshared;
#endif

struct logpack_t {
	volatile int logfd;
	pthread_mutex_t mutex;
	char* fname;
#ifdef LOGPACK_ENABLE_PSHARED
	logpack_shared* shared;
#endif
};

logpack_t* logpack_new(const char* fname)
{
	logpack_t* lpk = (logpack_t*)calloc(1, sizeof(logpack_t));
	if(!lpk) {
		goto err_calloc;
	}

	lpk->fname = strdup(fname);
	if(!lpk->fname) {
		goto err_fname;
	}

	if(pthread_mutex_init(&lpk->mutex, NULL) != 0) {
		goto err_mutex;
	}

	lpk->logfd = open(lpk->fname, O_WRONLY|O_APPEND|O_CREAT, 0640);
	if(lpk->logfd < 0) {
		goto err_logfd;
	}

#ifdef LOGPACK_ENABLE_PSHARED
	lpk->mapfd = -1;
#endif

	return lpk;

err_logfd:
	pthread_mutex_destroy(&lpk->mutex);
err_mutex:
	free(lpk->fname);
err_fname:
	free(lpk);
err_calloc:
	return NULL;
}

void logpack_free(logpack_t* lpk)
{
	close(lpk->logfd);
	free(lpk->fname);
	pthread_mutex_destroy(&lpk->mutex);
	free(lpk);
}

static inline int logpack_lock(logpack_t* lpk)
{
	if(pthread_mutex_lock(&lpk->mutex) != 0) {
		return -1;
	}
	return 0;
}

static inline void logpack_unlock(logpack_t* lpk)
{
	pthread_mutex_unlock(&lpk->mutex);
}

int logpack_reopen(logpack_t* lpk)
{
	if(logpack_lock(lpk) < 0) { return -1; }

	int tmp = open(lpk->fname, O_WRONLY|O_APPEND|O_CREAT, 0640);
	if(tmp < 0) {
		return -1;
	}

	close(lpk->logfd);
	lpk->logfd = tmp;

	logpack_unlock(lpk);
	return 0;
}

int logpack_write_raw(logpack_t* lpk, const char* buf, size_t size)
{
	int ret = 0;
	if(logpack_lock(lpk) < 0) { return -1; }

	while(true) {
		ssize_t rl = write(lpk->logfd, buf, size);
		if(rl <= 0) { ret = -1; break; }
		if((size_t)rl >= size) { break; }
		size -= rl;
	}

	logpack_unlock(lpk);
	return ret;
}


