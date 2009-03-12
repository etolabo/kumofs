#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <memory.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include <pthread.h>
#include <libmemcached/memcached.h>

extern char* optarg;
extern int optint, opterr, optopt;
const char* g_progname;

static enum {
	TEST_GET,
	TEST_SET,
} g_test;

static const char* g_host = "127.0.0.1";
static unsigned short g_port = 11211;

static uint32_t g_num_request;
static uint32_t g_num_thread;
static size_t g_keylen = 16;
static size_t g_vallen = 400;
static bool g_binary = false;
static pthread_mutex_t g_thread_lock;

#define KEY_FILL "k"
#define KEY_LENGTH (strlen(KEY_PREFIX)+8)

static struct timeval g_timer;

void reset_timer()
{
	gettimeofday(&g_timer, NULL);
}

void show_timer()
{
	size_t size_bytes = (KEY_LENGTH+g_vallen) * g_num_request * g_num_thread;
	size_t requests = g_num_request * g_num_thread;

	struct timeval endtime;
	double sec;
	gettimeofday(&endtime, NULL);
	sec = (endtime.tv_sec - g_timer.tv_sec)
		+ (double)(endtime.tv_usec - g_timer.tv_usec) / 1000 / 1000;
	printf("%f sec\n", sec);
	printf("%f MB\n", ((double)size_bytes)/1024/1024);
	printf("%f Mbps\n", ((double)size_bytes)*8/sec/1000/1000);
	printf("%f req/sec\n", ((double)requests)/sec);
}


static void usage()
{
	printf(
		"Usage: %s set [options]  <num threads>  <num entries>\n"
		"  set key0 up to key$(<num entries>) using <num threads> threads.\n"
		"  <num entries> must be a multile of <num threads>.\n"
		"    -l HOST    : memcached server address\n"
		"    -p PORT    : memcached server port\n"
		"    -b         : use binary protocol\n"
		"    -k         : size of key >= 8\n"
		"    -v         : size of value\n"
		"    -h         : print this help message\n"
		"\n"
		"Usage: %s get [options]  <num threads>  <num requests per thread>\n"
		"  each thread gets key0 up to key$(<num requests per thread>).\n"
		"  the keys have to be set initially.\n"
		"    -l HOST    : memcached server address\n"
		"    -p PORT    : memcached server port\n"
		"    -b         : use binary protocol\n"
		"    -k         : size of key >= 8\n"
		"    -v         : size of value\n"
		"    -h         : print this help message\n"
		, g_progname
		, g_progname);
	exit(1);
}

static void parse_argv(int argc, char* argv[])
{
	g_progname = argv[0];
	int c;
	while((c = getopt(argc, argv, "l:p:bkvh")) != -1) {
		switch(c) {
		case 'l':
			g_host = optarg;
			break;

		case 'p':
			g_port = atoi(optarg);
			break;

		case 'b':
			g_binary = true;
			break;

		case 'k':
			g_keylen = atoi(optarg);
			break;

		case 'v':
			g_vallen = atoi(optarg);
			break;

		case 'h': /* FALL THROUGH */
		case '?': /* FALL THROUGH */
		default:
			usage();
		}
	}
	
	argc -= optind;

	if(argc != 3) { usage(); }

	if(strcmp(argv[1], "get") == 0) {
		g_test = TEST_GET;
	} else if(strcmp(argv[1], "set") == 0) {
		g_test = TEST_SET;
	} else {
		usage();
	}

	g_num_thread  = atoi(argv[optind]);
	g_num_request = atoi(argv[optind+1]);

	if(g_test == TEST_SET && !g_num_request % g_num_thread != 0) {
		usage();
	}

	if(g_test == TEST_SET) {
		printf("set benchmark\n");
		printf("protocol type       : %s\n", g_binary ? "binary" : "text");
		printf("number of threads   : %u\n", g_num_thread);
		printf("number of entries   : %u\n", g_num_request);
		printf("key size            : %u\n", g_keylen);
		printf("value size          : %u\n", g_vallen);
	} else {
		printf("get benchmark\n");
		printf("protocol type       : %s\n", g_binary ? "binary" : "text");
		printf("number of threads   : %u\n", g_num_thread);
		printf("requests per thread : %u\n", g_num_request);
		printf("key size            : %u\n", g_keylen);
		printf("value size          : %u\n", g_vallen);
	}
}


static memcached_st* initialize_user()
{
	memcached_st* st = memcached_create(NULL);
	if(!st) {
		perror("memcached_create failed");
		exit(1);
	}

	memcached_server_add(st, g_host, g_port);
	if(g_binary) {
		memcached_behavior_set(st, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
	}

	return st;
}

inline void fill_key_prefix(char* keybuf)
{
	memset(keybuf, KEY_FILL, g_keylen-8);
}

inline void pack_keynum(char* keybuf, uint32_t i)
{
	// 0x40 - 0x4f is printable ascii character
	unsigned char* prefix = (unsigned char*)keybuf + strlen(KEY_PREFIX);
	prefix[0] = ((i >> 0) & 0x0f) + 0x40;
	prefix[1] = ((i >> 4) & 0x0f) + 0x40;
	prefix[2] = ((i >> 8) & 0x0f) + 0x40;
	prefix[3] = ((i >>12) & 0x0f) + 0x40;
	prefix[4] = ((i >>16) & 0x0f) + 0x40;
	prefix[5] = ((i >>20) & 0x0f) + 0x40;
	prefix[6] = ((i >>24) & 0x0f) + 0x40;
	prefix[7] = ((i >>28) & 0x0f) + 0x40;
}

static void* thread_set(void* segment)
{
	uint32_t s = *(uint32_t*)segment;
	uint32_t begin = g_num_request / s;
	uint32_t end   = begin + s;

	char keybuf[KEY_LENGTH];
	fill_key_prefix(keybuf);

	memcached_st* st = initialize_user();
	
	char* valbuf = malloc(g_vallen);
	if(!valbuf) {
		perror("malloc() failed");
		exit(1);
	}
	memset(valbuf, 0, g_vallen);

	pthread_mutex_lock(&g_thread_lock);
	pthread_mutex_unlock(&g_thread_lock);
	
	uint32_t i;
	for(i=begin; i < end; ++i) {
		pack_keynum(keybuf, i);
		memcached_return ret =
			memcached_set(st, keybuf, sizeof(keybuf), valbuf, g_vallen, 0, 0);
		if(ret != MEMCACHED_SUCCESS) {
			fprintf(stderr, "put failed: %s\n", memcached_strerror(st, ret));
		}
	}
	
	free(valbuf);
	memcached_free(st);
	return NULL;
}

static void* thread_get(void* trash)
{
	char keybuf[KEY_LENGTH];
	memcpy(keybuf, KEY_PREFIX, strlen(KEY_PREFIX));

	memcached_st* st = initialize_user();

	pthread_mutex_lock(&g_thread_lock);
	pthread_mutex_unlock(&g_thread_lock);

	uint32_t i;
	for(i=0; i < g_num_request; ++i) {
		pack_keynum(keybuf, i);
		size_t vallen;
		uint32_t flags;
		memcached_return ret;
		char* value = memcached_get(st, keybuf, sizeof(keybuf),
				&vallen, &flags, &ret);
		if(ret != MEMCACHED_SUCCESS) {
			fprintf(stderr, "get failed: %s\n", memcached_strerror(st, ret));
		} else if(!value) {
			fprintf(stderr, "get failed: key not found\n");
		}
	}

	memcached_free(st);
	return NULL;
}

int main(int argc, char* argv[])
{
	uint32_t i;
	memcached_return ret;

	parse_argv(argc, argv);	

	signal(SIGPIPE, SIG_IGN);


	printf("starting threads ...\n");

	pthread_mutex_init(&g_thread_lock, NULL);
	pthread_mutex_lock(&g_thread_lock);

	void* (func)(void*);
	switch(g_test) {
	case TEST_GET:
		func = thread_get; break;
	case TEST_SET:
		func = thread_set; break;
	}

	pthread_t threads[g_num_thread];
	for(i=0; i < g_num_thread; ++i) {
		int err = pthread_create(&threads[i], NULL, func, (void*)&i);
		if(err != 0) {
			fprintf(stderr, "failed to create thread: %s\n", strerror(err));
			exit(1);
		}
	}

	pthread_yield();
	pthread_yield();
	printf("start benchmark ...\n");

	reset_timer();
	pthread_mutex_unlock(&g_thread_lock);

	for(i=0; i < g_num_thread; ++i) {
		void* ret;
		int err = pthread_join(threads[i], &ret);
		if(err != 0) {
			fprintf(stderr, "failed to join thread: %s\n", strerror(err));
		}
	}
	show_timer();

	return 0;
}

