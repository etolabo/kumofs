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
	pthread_mutex_t mutex;
#ifdef LOGPACK_ENABLE_PSHARED_ROBUST
	volatile int seqnum;
#endif
} logpack_pshared;
#endif

struct logpack_t {
	volatile int logfd;
	pthread_mutex_t mutex;
	char* fname;
#ifdef LOGPACK_ENABLE_PSHARED
	int mapfd;
	logpack_pshared* pshared;
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

#ifdef LOGPACK_ENABLE_PSHARED
static int logpack_open_pshared(const char* basename)
{
	int mapfd;
	size_t flen = strlen(basename);
	char* tmpname;

	tmpname = (char*)malloc(flen+8);
	if(!tmpname) {
		return -1;
	}

	memcpy(tmpname, basename, flen);
	memcpy(tmpname+flen, "-XXXXXX", 8);  // '-XXXXXX' + 1(='\0')

	mapfd = mkstemp(tmpname);
	if(mapfd < 0) {
		free(tmpname);
		return -1;
	}

	if(ftruncate(mapfd, sizeof(logpack_t)) < 0 ) {
		unlink(tmpname);
		free(tmpname);
		return -1;
	}

	unlink(tmpname);

	return mapfd;
}

logpack_t* logpack_new(const char* fname)
{
	pthread_mutexattr_t attr;

	logpack_t* lpk = (logpack_t*)calloc(1, sizeof(logpack_t));
	if(!lpk) {
		goto err_calloc;
	}

	lpk->fname = strdup(fname);
	if(!lpk->fname) {
		goto err_fname;
	}

	lpk->mapfd = logpack_open_pshared(fname);
	if(lpk->mapfd < 0) {
		goto err_open_pshared;
	}

	lpk->pshared = (logpack_pshared*)mmap(NULL, sizeof(logpack_pshared),
			PROT_READ|PROT_WRITE, MAP_SHARED, mapfd, 0);
	if(lpk->pshared == MAP_FAILED) {
		goto err_mmap;
	}
	memset(lpk->pshared, 0, sizeof(logpack_pshared));

	if(pthread_mutexattr_init(&attr) != 0) {
		goto err_mutexattr;
	}
#ifdef LOGPACK_ENABLE_PSHARED_ROBUST
	if(pthread_mutexattr_setrobust_np(&attr, PTHREAD_MUTEX_ROBUST_NP) != 0) {
		goto err_mutexattr_set;
	}
#endif
	if(pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
		goto err_mutexattr_set;
	}

	memset(&lpk->mutex, 0, sizeof(pthread_mutex_t));
	if(pthread_mutex_init(&lpk->mutex, &attr) != 0) {
		goto err_mutexattr_set;
	}

	lpk->logfd = open(lpk->fname, O_WRONLY|O_APPEND|O_CREAT, 0640);
	if(lpk->logfd < 0) {
		goto err_logfd;
	}

	return lpk;

err_logfd:
	pthread_mutex_destroy(&lpk->mutex);
err_mutexattr_set:
	pthread_mutexattr_destroy(&attr);
err_mutexattr:
	munmap(lpk->pshared, sizeof(logpack_pshared));
err_mmap:
	close(lpk->mapfd);
err_open_pshared:
	free(lpk->fname);
err_fname:
	free(lpk);
err_calloc:
	return NULL;
}
#endif

void logpack_free(logpack_t* lpk)
{
	close(lpk->logfd);
	free(lpk->fname);
#ifdef LOGPACK_ENABLE_PSHARED
	if(lpk->pshared) {
		munmap(lpk->pshared, sizeof(logpack_pshared));
		close(lpk->mapfd);
	} else {
		pthread_mutex_destroy(&lpk->mutex);
	}
#else
	pthread_mutex_destroy(&lpk->mutex);
#endif
	free(lpk);
}

static inline int logpack_lock(logpack_t* lpk)
{
#ifdef LOGPACK_ENABLE_PSHARED
#ifdef LOGPACK_ENABLE_PSHARED_ROBUST
retry:
	int seqnum = lpk->pshared->seqnum;
#endif
#endif

#ifdef LOGPACK_ENABLE_PSHARED
	if(lpk->pshared) {
		if(pthread_mutex_lock(&lpk->pshared->mutex) != 0) {
#ifdef LOGPACK_ENABLE_PSHARED_ROBUST
			if(errno == EOWNERDEAD) {
				if(__sync_bool_compare_and_swap(&lpk->pshared->seqnum,
							seqnum, seqnum+1)) {
					if(pthread_mutex_consistent_np(&lpk->mutex) != 0) {
						return -1;
					}
				}
				goto retry;
			}
#endif
			return -1;
		}
		return 0;
	}
#endif

	if(pthread_mutex_lock(&lpk->mutex) != 0) {
		return -1;
	}
	return 0;
}

static inline void logpack_unlock(logpack_t* lpk)
{
#ifdef LOGPACK_ENABLE_PSHARED
	if(lpk->pshared) {
		pthread_mutex_unlock(&lpk->pshared->mutex);
		return;
	}
#endif
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


