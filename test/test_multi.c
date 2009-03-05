#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <getopt.h>

#include "libmemcached/memcached.h"

#define KEY_PREFIX "key"
#define VAL_PREFIX "val"

void usage(void)
{
	printf("usage: ./test <host> <port> <num>\n");
	exit(1);
}

void pexit(const char* msg)
{
	perror(msg);
	exit(1);
}

int main(int argc, char* argv[])
{
	if(argc < 4) { usage(); }

	char* host = argv[1];

	unsigned short port = atoi(argv[2]);
	if(port == 0) { usage(); }

	uint32_t num = atoi(argv[3]);
	if(num == 0) { usage(); }

	memcached_st* mc = memcached_create(NULL);
	if(mc == NULL) { pexit("memcached_create"); }

	memcached_server_add(mc, host, port);

	memcached_behavior_set(mc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
	//memcached_behavior_set(mc, MEMCACHED_BEHAVIOR_POLL_TIMEOUT, 20*1000);
	//memcached_behavior_set(mc, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, 20*1000);

	char kbuf[strlen(KEY_PREFIX) + 11];
	char vbuf[strlen(VAL_PREFIX) + 11];

	uint32_t i;
	for(i=0; i < num; ++i) {
		int klen = sprintf(kbuf, KEY_PREFIX "%d", i);
		int vlen = sprintf(vbuf, VAL_PREFIX "%d", i);
		printf("set '%s' = '%s'\n", kbuf, vbuf);
		memcached_set(mc, kbuf, klen, vbuf, vlen, 0, 0);
	}

	static const int multi = 10;

	char rmkbuf[multi][strlen(KEY_PREFIX) + 11];
	char rmvbuf[multi][strlen(VAL_PREFIX) + 11];
	size_t mklen[multi];
	size_t mvlen[multi];
	int j;
	char* mkbuf[multi];
	char* mvbuf[multi];
	for(j=0; j < multi; ++j) {
		mkbuf[j] = rmkbuf[j];
		mvbuf[j] = rmvbuf[j];
	}
while(1) {
	for(i=0; i < num-multi; ++i) {
		for(j=0; j < multi; ++j) {
			mklen[j] = sprintf(mkbuf[j], KEY_PREFIX "%d", i+j);
			mvlen[j] = sprintf(mvbuf[j], VAL_PREFIX "%d", i+j);
		}
		memcached_return rc;
		printf("get %s %s\n", mkbuf[0], mkbuf[multi-1]);
		rc = memcached_mget(mc, mkbuf, mklen, multi);
		if(rc != MEMCACHED_SUCCESS) {
			fprintf(stderr, "mget failed %s\n", memcached_strerror(mc, rc));
			continue;
		}
		int n = 0;
		while(1) {
			size_t keylen = sizeof(kbuf);
			size_t vallen;
			uint32_t flags;
			char* val = memcached_fetch(mc, kbuf, &keylen, &vallen, &flags, &rc);
			if(val == NULL) { break; }
			if(rc != MEMCACHED_SUCCESS) {
				fprintf(stderr, "fetch failed %s\n", memcached_strerror(mc, rc));
				break;
			}
			int matched = 0;
			for(j=0; j < multi; ++j) {
				if(keylen == mklen[j] && memcmp(mkbuf[j], kbuf, keylen) == 0) {
					if(vallen != mvlen[j] || memcmp(mvbuf[j], val, vallen) != 0) {
						fprintf(stderr, "** key '%s' not match ** '", mkbuf[j]);
						fwrite(val, vallen, 1, stderr);
						fprintf(stderr, "'\n");
					} else {
						printf("fetch '%s' = '", mkbuf[j]);
						fwrite(val, vallen, 1, stdout);
						printf("'\n");
					}
					n += (j+1);
					matched = 1;
					break;
				}
			}
			if(!matched) {
				fprintf(stderr, "** unexpected key '");
				fwrite(kbuf, keylen, 1, stderr);
				fprintf(stderr, "'\n");
			}
		}

		int ok = 0;  for(j=0; j < multi; ++j) { ok += (j+1); }
		if(ok != n) {
			fprintf(stderr, "** some keys are not found **\n");
		} else {
			printf("mget ok\n");
		}
	}
}

	return 0;
}

