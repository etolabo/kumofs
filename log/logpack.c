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

struct logpack_t {
	volatile off_t off;
	volatile int fd;
	off_t lotate_size;
	pthread_mutex_t mutex;
	char* namebuf;
	size_t namelen;
#ifdef LOGPACK_ENABLE_PSHARED
	int mapfd;
	volatile int rec;
#endif
};

static int open_logfile(logpack_t* lpk)
{
	time_t ti = time(NULL);
	struct tm t;
	localtime_r(&ti, &t);
	strftime(lpk->namebuf+lpk->namelen, 13, "-%Y-%m-%d.", &t);

	unsigned short n;
	for(n=0; n < 9999; ++n) {
		sprintf(lpk->namebuf+lpk->namelen+12, "%04d", n);

		int fd = open(lpk->namebuf, O_WRONLY|O_CREAT|O_EXCL, 0666);
		if(fd < 0) {
			if(errno == EEXIST) { continue; }
			free(lpk->namebuf);
			return -1;
		}

		return fd;
	}

	return -1;
}

logpack_t* logpack_new(const char* basename, size_t lotate_size)
{
	logpack_t* lpk = (logpack_t*)calloc(1, sizeof(logpack_t));
	if(!lpk) {
		goto err_calloc;
	}

	if(pthread_mutex_init(&lpk->mutex, NULL) != 0) {
		goto err_mutex;
	}

	lpk->namelen = strlen(basename);
	lpk->namebuf = (char*)malloc(lpk->namelen + 17);  /* 17: "-YYYY-mm.dd.nnnn\0" */
	if(!lpk->namebuf) {
		goto err_namebuf;
	}
	memcpy(lpk->namebuf, basename, lpk->namelen);

	lpk->fd = open_logfile(lpk);
	if(lpk->fd < 0) {
		goto err_openfd;
	}

	lpk->lotate_size = lotate_size;
#ifdef LOGPACK_ENABLE_PSHARED
	lpk->mapfd = -1;
#endif

	return lpk;

err_openfd:
	free(lpk->namebuf);
err_namebuf:
	pthread_mutex_destroy(&lpk->mutex);
err_mutex:
	free(lpk);
err_calloc:
	return NULL;
}

#ifdef LOGPACK_ENABLE_PSHARED
logpack_t* logpack_new_pshared(const char* basename, size_t lotate_size)
{
	int mapfd;
	logpack_t* lpk;
	pthread_mutexattr_t attr;

	const size_t namelen = strlen(basename);
	char* namebuf = (char*)malloc(namelen + 17);  /* 17: "-YYYY-mm.dd.nnnn\0" */
	if(!namebuf) {
		goto err_namebuf;
	}
	memcpy(namebuf, basename, namelen);

	memcpy(namebuf+namelen, "-XXXXXX", 8);  // '-XXXXXX' + 1(='\0')
	mapfd = ::mkstemp(namebuf);
	if(mapfd < 0) {
		goto err_mkstemp;
	}
	if(ftruncate(mapfd, sizeof(logpack_t)) < 0 ) {
		goto err_mkstemp;
	}
	unlink(namebuf);

	lpk = (logpack_t*)mmap(NULL, sizeof(logpack_t),
			PROT_READ|PROT_WRITE, MAP_SHARED, mapfd, 0);
	if(lpk == MAP_FAILED) {
		goto err_mmap;
	}
	memset(lpk, 0, sizeof(logpack_t));

	if(pthread_mutexattr_init(&attr) != 0) {
		goto err_mutexattr;
	}
	if(pthread_mutexattr_setrobust_np(&attr, PTHREAD_MUTEX_ROBUST_NP) != 0) {
		goto err_mutexattr_set;
	}
	if(pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
		goto err_mutexattr_set;
	}

	memset(&lpk->mutex, 0, sizeof(pthread_mutex_t));
	if(pthread_mutex_init(&lpk->mutex, &attr) != 0) {
		goto err_mutexattr_set;
		return NULL;
	}

	lpk->namebuf = namebuf;
	lpk->namelen = namelen;

	lpk->fd = open_logfile(lpk);
	if(lpk->fd < 0) {
		goto err_openfd;
	}

	lpk->lotate_size = lotate_size;
	lpk->mapfd = mapfd;

	pthread_mutexattr_destroy(&attr);
	return lpk;

err_openfd:
	pthread_mutex_destroy(&lpk->mutex);
err_mutexattr_set:
	pthread_mutexattr_destroy(&attr);
err_mutexattr:
	munmap(lpk, sizeof(logpack_t));
err_mmap:
	close(mapfd);
err_mkstemp:
	free(namebuf);
err_namebuf:
	return NULL;
}
#endif

void logpack_free(logpack_t* lpk)
{
	close(lpk->fd);
	free(lpk->namebuf);
#ifdef LOGPACK_ENABLE_PSHARED
	if(lpk->mapfd < 0) {
#endif
		pthread_mutex_destroy(&lpk->mutex);
		free(lpk);
#ifdef LOGPACK_ENABLE_PSHARED
	} else {
		int mapfd = lpk->mapfd;
		munmap(lpk, sizeof(logpack_t));
		close(mapfd);
	}
#endif
}

static int logpack_lotate_log(logpack_t* lpk)
{
#ifdef LOGPACK_ENABLE_PSHARED
retry:
	int rec = lpk->rec;
#endif
	if(pthread_mutex_lock(&lpk->mutex) != 0) {
#ifdef LOGPACK_ENABLE_PSHARED
		if(errno == EOWNERDEAD) {
			if(__sync_bool_compare_and_swap(&lpk->rec, rec, rec+1)) {
				if(pthread_mutex_consistent_np(&lpk->mutex) != 0) {
					return -1;
				}
			}
			goto retry;
		}
#endif
		return -1;
	}
	if(lpk->off <= lpk->lotate_size) {
		pthread_mutex_unlock(&lpk->mutex);
		return 0;
	}
	int nfd = open_logfile(lpk);
	if(nfd < 0) {
		pthread_mutex_unlock(&lpk->mutex);
		return -1;
	}
	lpk->fd = nfd;
	lpk->off = 0;
	pthread_mutex_unlock(&lpk->mutex);
	return 0;
}

int logpack_write_raw(logpack_t* lpk, const char* buf, size_t size)
{
	if(lpk->off > lpk->lotate_size) {
		if(logpack_lotate_log(lpk) < 0) {
			return -1;
		}
	}

	off_t off = __sync_fetch_and_add(&lpk->off, size);

	while(true) {
		ssize_t rl = pwrite(lpk->fd, buf, size, off);
		if(rl <= 0) { return -1; }
		if((size_t)rl >= size) { return 0; }
		size -= rl;
		buf  += rl;
		off  += rl;
	}
}


#if 0
int logpack_log_init(logpack_log_t* pac, const char* name, uint16_t version)
{
	msgpack_sbuffer_init(&pac->buffer);
	uint64_t zero = 0;
	if(msgpack_sbuffer_write(&zero, 8) < 0) {
		msgpack_sbuffer_destroy(&pac->buffer);
		return -1;
	}
	msgpack_pack_init(&pac->packer, &pac->buffer, msgpack_sbuffer_write);
}

void logpack_log_destroy(logpack_log_t* pac)
{
	msgpack_sbuffer_destroy(&pac->buffer);
}

int logpack_write(logpack_t* lpk, const logpack_log_t* pac)
{
	return logpack_write_raw(lpk, pac->buffer.ptr, pac->buffer.size);
}
#endif


