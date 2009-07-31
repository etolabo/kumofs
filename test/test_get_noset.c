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
	printf("usage: ./test_get_noset <host> <port> <num>\n");
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

//	memcached_behavior_set(mc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);

	char kbuf[strlen(KEY_PREFIX) + 11];
	char vbuf[strlen(VAL_PREFIX) + 11];

	uint32_t i;

while(1) {
	for(i=0; i < num; ++i) {
		int klen = sprintf(kbuf, KEY_PREFIX "%d", i);
		int vlen = sprintf(vbuf, VAL_PREFIX "%d", i);
		size_t vallen;
		uint32_t flags;
		memcached_return rc;
		char* val = memcached_get(mc, kbuf, klen, &vallen, &flags, &rc);
		if(!val) {
			fprintf(stderr, "** key '%s' not found **\n", kbuf);
		} else if(vallen != vlen || memcmp(val, vbuf, vlen) != 0) {
			fprintf(stderr, "** key '%s' not match ** '", kbuf);
			fwrite(val, vallen, 1, stderr);
			fprintf(stderr, "'\n");
		} else {
			printf("get '%s' = '", kbuf);
			fwrite(val, vallen, 1, stdout);
			printf("'\n");
		}
	}
}

	return 0;
}

