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

static const char* g_host = "127.0.0.1";
static unsigned short g_port = 11211;

static unsigned long g_num_request;
static unsigned long g_num_multiplex = 1;
static unsigned long g_num_thread = 1;
static unsigned long g_keylen = 8;
static unsigned long g_vallen = 1024;
static bool g_binary = false;
static bool g_noset = false;
static bool g_noget = false;

static pthread_mutex_t g_count_lock;
static pthread_cond_t  g_count_cond;
static volatile int g_thread_count;
static pthread_mutex_t g_thread_lock;

#define KEY_FILL 'k'

static struct timeval g_timer;

void reset_timer()
{
	gettimeofday(&g_timer, NULL);
}

void show_timer()
{
	struct timeval endtime;
	double sec;
	unsigned long size_bytes = (g_keylen+g_vallen) * g_num_request * g_num_thread;
	unsigned long requests = g_num_request * g_num_thread;
	gettimeofday(&endtime, NULL);
	sec = (endtime.tv_sec - g_timer.tv_sec)
		+ (double)(endtime.tv_usec - g_timer.tv_usec) / 1000 / 1000;
	printf("%f sec\n", sec);
	printf("%f MB\n", ((double)size_bytes)/1024/1024);
	printf("%f Mbps\n", ((double)size_bytes)*8/sec/1000/1000);
	printf("%f req/sec\n", ((double)requests)/sec);
	printf("%f usec/req\n", ((double)sec)/requests*1000*1000);
}


static pthread_t* create_worker(void* (*func)(void*))
{
	unsigned long i;
	pthread_t* threads = malloc(sizeof(pthread_t)*g_num_thread);

	pthread_mutex_lock(&g_thread_lock);
	g_thread_count = 0;

	for(i=0; i < g_num_thread; ++i) {
		int err = pthread_create(&threads[i], NULL, func, NULL);
		if(err != 0) {
			fprintf(stderr, "failed to create thread: %s\n", strerror(err));
			exit(1);
		}
	}

	pthread_mutex_lock(&g_count_lock);
	while(g_thread_count < g_num_thread) {
		pthread_cond_wait(&g_count_cond, &g_count_lock);
	}
	pthread_mutex_unlock(&g_count_lock);

	return threads;
}

static void start_worker()
{
	pthread_mutex_unlock(&g_thread_lock);
}

static void join_worker(pthread_t* threads)
{
	unsigned long i;
	for(i=0; i < g_num_thread; ++i) {
		void* ret;
		int err = pthread_join(threads[i], &ret);
		if(err != 0) {
			fprintf(stderr, "failed to join thread: %s\n", strerror(err));
		}
	}
}

static unsigned long wait_worker_ready()
{
	unsigned long index;
	pthread_mutex_lock(&g_count_lock);
	index = g_thread_count++;
	pthread_cond_signal(&g_count_cond);
	pthread_mutex_unlock(&g_count_lock);
	pthread_mutex_lock(&g_thread_lock);
	pthread_mutex_unlock(&g_thread_lock);
	return index;
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

	//memcached_behavior_set(st, MEMCACHED_BEHAVIOR_POLL_TIMEOUT, 20*1000);
	//memcached_behavior_set(st, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, 20*1000);

	return st;
}

static char* malloc_keybuf()
{
	char* keybuf = malloc(g_keylen+1);
	if(!keybuf) {
		perror("malloc for key failed");
		exit(1);
	}
	memset(keybuf, KEY_FILL, g_keylen);
	keybuf[g_keylen] = '\0';
	return keybuf;
}

static char* malloc_valbuf()
{
	char* valbuf = malloc(g_vallen);
	if(!valbuf) {
		perror("malloc for value failed");
		exit(1);
	}
	memset(valbuf, 'v', g_vallen);
	//memset(valbuf, 0, g_vallen);
	return valbuf;
}

static void pack_keynum(char* keybuf, uint32_t i)
{
	/* 0x40 - 0x4f is printable ascii character */
	unsigned char* prefix = (unsigned char*)keybuf + g_keylen - 8;
	prefix[0] = ((i >> 0) & 0x0f) + 0x40;
	prefix[1] = ((i >> 4) & 0x0f) + 0x40;
	prefix[2] = ((i >> 8) & 0x0f) + 0x40;
	prefix[3] = ((i >>12) & 0x0f) + 0x40;
	prefix[4] = ((i >>16) & 0x0f) + 0x40;
	prefix[5] = ((i >>20) & 0x0f) + 0x40;
	prefix[6] = ((i >>24) & 0x0f) + 0x40;
	prefix[7] = ((i >>28) & 0x0f) + 0x40;
}

static void* worker_set(void* trash)
{
	unsigned long i, t;
	memcached_return ret;
	memcached_st* st = initialize_user();
	char* keybuf = malloc_keybuf();
	char* valbuf = malloc_valbuf();
	
	printf("s");
	t = wait_worker_ready();

	for(i=t*g_num_request, t=i+g_num_request; i < t; ++i) {
		pack_keynum(keybuf, i);
		ret = memcached_set(st, keybuf, g_keylen, valbuf, g_vallen, 0, 0);
		if(ret != MEMCACHED_SUCCESS) {
			fprintf(stderr, "set failed: %s\n", memcached_strerror(st, ret));
		}
	}
	
	free(keybuf);
	free(valbuf);
	memcached_free(st);
	return NULL;
}

static void* worker_get(void* trash)
{
	unsigned long i, t, m;
	memcached_st* st = initialize_user();
	char* mkeybuf[g_num_multiplex];
	size_t mkeylen[g_num_multiplex];
	char* value;
	size_t vallen;
	uint32_t flags;
	memcached_return ret;

	for(m=0; m < g_num_multiplex; ++m) {
		mkeybuf[m] = malloc_keybuf();
	}

	printf("g");
	t = wait_worker_ready();

	if(g_num_multiplex == 1) {
		char* keybuf = mkeybuf[0];
		for(i=t*g_num_request, t=i+g_num_request; i < t; ++i) {
			pack_keynum(keybuf, i);
			value = memcached_get(st, keybuf, g_keylen,
					&vallen, &flags, &ret);
			if(ret != MEMCACHED_SUCCESS) {
				fprintf(stderr, "get failed: %s\n", memcached_strerror(st, ret));
			} else if(!value) {
				fprintf(stderr, "get failed: key not found\n");
			}
			free(value);
		}
	} else {
		for(i=t*g_num_request, t=i+g_num_request; i < t; ) {

			for(m=0; m < g_num_multiplex; ++m) {
				pack_keynum(mkeybuf[m], i+m);
				mkeylen[m] = g_keylen;
			}

			ret = memcached_mget(st, mkeybuf, mkeylen, g_num_multiplex);
			if (ret != MEMCACHED_SUCCESS) {
				fprintf(stderr, "mget failed: %s\n", memcached_strerror(st, ret));
				goto err;
			}

			for(m=0; true; ++m) {
				value = memcached_fetch(st, mkeybuf[m], &mkeylen[m],
								&vallen, &flags, &ret);
				if(!value) {
					break;
				}

				if(ret != MEMCACHED_SUCCESS) {
					fprintf(stderr, "get failed: %s\n", memcached_strerror(st, ret));
				}

				if(mkeylen[m] != g_keylen) {
					fprintf(stderr, "key length not match: expect %lu but got %lu\n",
							g_keylen, mkeylen[m]);
				}

				free(value);
			}

			if(m != g_num_multiplex) {
				fprintf(stderr, "num keys not match: expect %lu but got %lu\n",
						g_num_multiplex, m);
			}

			err:
			i += g_num_multiplex;
		}
	}

	for(m=0; m < g_num_multiplex; ++m) {
		free(mkeybuf[m]);
	}
	memcached_free(st);
	return NULL;
}


static void usage(const char* msg)
{
	printf("Usage: %s [options]    <num requests>\n"
		" -l HOST=127.0.0.1  : memcached server address\n"
		" -p PORT=11211      : memcached server port\n"
		" -k SIZE=8          : size of key >= 8\n"
		" -v SIZE=1024       : size of value\n"
		" -m NUM=1           : get multiplex\n"
		" -t NUM=1           : number of threads\n"
		" -b                 : use binary protocol\n"
		" -g                 : omit to set values\n"
		" -s                 : omit to get benchmark\n"
		" -h                 : print this help message\n"
		, g_progname);
	if(msg) { printf("error: %s\n", msg); }
	exit(1);
}

static void parse_argv(int argc, char* argv[])
{
	int c;
	g_progname = argv[0];
	while((c = getopt(argc, argv, "hbgsl:p:k:v:m:t:")) != -1) {
		switch(c) {
		case 'l':
			g_host = optarg;
			break;

		case 'p':
			g_port = atoi(optarg);
			if(g_port == 0) { usage("invalid port number"); }
			break;

		case 'k':
			g_keylen = atoi(optarg);
			if(g_keylen < 8) { usage("invalid key size"); }
			break;

		case 'v':
			g_vallen  = atoi(optarg);
			if(g_vallen == 0) { usage("invalid value size"); }
			break;

		case 'b':
			g_binary = true;
			break;

		case 'm':
			g_num_multiplex  = atoi(optarg);
			break;

		case 't':
			g_num_thread  = atoi(optarg);
			break;

		case 'g':
			g_noset = true;
			break;

		case 's':
			g_noget = true;
			break;

		case 'h': /* FALL THROUGH */
		case '?': /* FALL THROUGH */
		default:
			usage(NULL);
		}
	}
	
	argc -= optind;

	if(argc != 1) { usage(NULL); }

	unsigned long multiplex_request = atoi(argv[optind]) / g_num_thread / g_num_multiplex;
	g_num_request = multiplex_request * g_num_multiplex;

	if(g_num_request == 0) { usage("invalid number of request"); }

	printf("number of threads    : %lu\n", g_num_thread);
	printf("number of requests   : %lu\n", g_num_thread * g_num_request);
	printf("requests per thread  : %lu\n", g_num_request);
	printf("get multiplex        : %lu\n", g_num_multiplex);
	printf("size of key          : %lu bytes\n", g_keylen);
	printf("size of value        : %lu bytes\n", g_vallen);
}

int main(int argc, char* argv[])
{
	pthread_t* threads;

	parse_argv(argc, argv);	

	signal(SIGPIPE, SIG_IGN);

	pthread_mutex_init(&g_count_lock, NULL);
	pthread_cond_init(&g_count_cond, NULL);
	pthread_mutex_init(&g_thread_lock, NULL);

	if(!g_noset) {
		printf("----\n[");
		threads = create_worker(worker_set);
		reset_timer();
		printf("] ...\n");
		start_worker();
		join_worker(threads);
		show_timer();
	}

	if(!g_noget) {
		printf("----\n[");
		threads = create_worker(worker_get);
		reset_timer();
		printf("] ...\n");
		start_worker();
		join_worker(threads);
		show_timer();
	}

	return 0;
}

