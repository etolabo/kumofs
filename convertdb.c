#include <tchdb.h>
#include <tcutil.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static TCHDB* dstdb = NULL;
static TCHDB* srcdb = NULL;

static uint64_t success = 0;
static uint64_t deleted = 0;
static uint64_t invalid = 0;

static char* tmpbuf = NULL;
static size_t tmpsz = 0;

void die_if(bool cond, const char* msg)
{
	if(!cond) { return; }

	fprintf(stderr, "error: %s. Abort.\n", msg);
	if(dstdb) {
		tchdbclose(dstdb);
		tchdbdel(dstdb);
	}
	if(srcdb) {
		tchdbclose(srcdb);
		tchdbdel(srcdb);
	}
	exit(1);
}

static bool iter(const void* kbuf, int ksiz, const void* vbuf, int vsiz, void* op)
{
	if(vsiz == 8) {
		++deleted;

	} else if(vsiz < 16) {
		++invalid;

	} else {
		if(tmpsz < vsiz-6) {
			tmpsz = (vsiz/1024+1)*1024;
			tmpbuf = realloc(tmpbuf, tmpsz);
			die_if(!tmpbuf, "realloc failed");
		}

		memcpy(tmpbuf,    vbuf, 10);
		memcpy(tmpbuf+10, vbuf+16, vsiz-16);

		if(!tchdbputasync(dstdb, kbuf, ksiz, tmpbuf, vsiz-6)) {
			fprintf(stderr, "tchdbput failed! key='");
			fwrite(kbuf, ksiz, 1, stderr);
			fprintf(stderr, "'\n");
		}

		++success;
	}

	return true;
}

int main(int argc, char* argv[])
{
	if(argc != 3) {
		printf("This tool converts kumofs-r1162 tchdb into kumofs-r1163 tchdb\n\n");
		printf("usage: %s  <src.tch>  <DST.tch>\n", argv[0]);
		printf(" Note: %s   src.tch -> DST.tch \n", argv[0]);
		exit(1);
	}

	const char* src = argv[1];
	const char* dst = argv[2];

	srcdb = tchdbnew();
	dstdb = tchdbnew();
	die_if(!srcdb, "src db init failed");
	die_if(!dstdb, "dst db init failed");

	die_if(!tchdbopen(srcdb, src, HDBOREADER), "Can't open src db");
	die_if(!tchdbopen(dstdb, dst, HDBOWRITER|HDBOCREAT), "Can't open dst db");

	die_if(tchdbrnum(dstdb) != 0, "dst db is not empty");
	printf("convert %lu entries ...\n", tchdbrnum(srcdb));

	tchdbforeach(srcdb, iter, NULL);

	printf("done. closing database ...\n");

	die_if(!tchdbclose(srcdb), "failed to close src db");
	die_if(!tchdbclose(dstdb), "failed to close dst db");

	uint64_t total = success + deleted + invalid;
	printf("convert       : %lu entries\n", success);
	printf("delete-flags  : %lu entries\n", deleted);
	printf("invalid value : %lu entries\n", invalid);
	printf("total         : %lu entries\n", total);

	return 0;
}

