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
		return NULL;
	}

	if(pthread_mutex_init(&lpk->mutex, NULL) != 0) {
		free(lpk);
		return NULL;
	}

	lpk->namelen = strlen(basename);
	lpk->namebuf = (char*)malloc(lpk->namelen + 17);  /* 17: "-YYYY-mm.dd.nnnn\0" */
	if(!lpk->namebuf) {
		pthread_mutex_destroy(&lpk->mutex);
		free(lpk);
		return NULL;
	}
	memcpy(lpk->namebuf, basename, lpk->namelen);

	lpk->fd = open_logfile(lpk);
	if(lpk->fd < 0) {
		free(lpk->namebuf);
		pthread_mutex_destroy(&lpk->mutex);
		free(lpk);
		return NULL;
	}

	lpk->lotate_size = lotate_size;

	return lpk;
}

void logpack_free(logpack_t* lpk)
{
	close(lpk->fd);
	free(lpk->namebuf);
	pthread_mutex_destroy(&lpk->mutex);
	free(lpk);
}

static int logpack_lotate_log(logpack_t* lpk)
{
	if(pthread_mutex_lock(&lpk->mutex) != 0) {
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


