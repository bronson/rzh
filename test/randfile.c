/* randfile.c
 * Scott Bronson
 * 4 Nov 2004
 *
 * Like randfile.pl but much, much faster.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include "mt19937ar.h"

#define VERSION "0.3"

#define exit_cmdline_error 1

static int opt_check = 0;
static int opt_seed = 1;
static int opt_size = 1024;


// reads as much as it can.  sets feof if it hit the eof.

size_t read_buffer(int fd, char *buf, size_t maxsiz, int *feof)
{
	int count = 0;
	*feof = 0;

	do {
		int ret = read(fd, buf+count, maxsiz-count);
		if(ret < 0) {
			perror("Could not read");
			exit(2);
		}
		if(ret == 0) {
			*feof = 1;
			break;
		}
		count += ret;
	} while(count < maxsiz);

	assert(count <= maxsiz);
	return count;
}


// writes the entire buffer or will die trying.

void write_buffer(int fd, char *buf, size_t count)
{
	do {
		size_t ret = write(fd, buf, count);
		if(ret < 0) {
			perror("Could not write");
			exit(2);
		}
		count -= ret;
	} while(count > 0);
}


typedef union {
	u_int32_t i;
	char c[4];
} intbuf;


void byte_check(u_int32_t ina, u_int32_t inb, size_t offset, int limit)
{
	int i;
	intbuf a;
	intbuf b;

	a.i = ina;
	b.i = inb;

	assert(limit > 0);
	assert(limit <= 4);

	for(i=0; i < limit; i++) {
		if(a.c[i] != b.c[i]) {
			fprintf(stderr, "Data doesn't match at byte %d\n", offset + i);
			exit(4);
		}
	}
}


void check_data(int fd, size_t incnt)
{
	char buf[BUFSIZ];
	u_int32_t *p;
	u_int32_t *e = (u_int32_t*)(buf + BUFSIZ);
	int feof;
	size_t count;
	size_t size;
	int rem = 0;

	do {
		size = read_buffer(fd, buf, BUFSIZ, &feof);
		rem = size & 3;

		if(size < BUFSIZ) {
			// ensure that e is still int-aligned
			e = (u_int32_t*)(buf + (size & ~3));
		}

		// check int-sized chunks
		for(p = (u_int32_t*)buf; p < e; p++) {
			// Using the RNG doubles the runtime for large data sets on my
			// system.  118MB takes 0.977 sec w/o, and 1.874 sec with.
			int rand = genrand_int32();
			if(*p != rand) {
				byte_check(*p, rand, (char*)p - buf + count, 4);
				assert(0);
			}
		}
		assert(p == e);

		// check the remainder
		if(rem) {
			// we only allow remainders at the very end of the file
			// because otherwise RNG synchronization would be wrong.
			assert(feof);
			byte_check(*e, genrand_int32(), (char*)e - buf + count, rem);
		}

		count += size;
		if(count > incnt) {
			fprintf(stderr, "Too much input: expected only %d bytes.\n", incnt);
			exit(3);
		}
	} while(!feof);

	if(count < incnt) {
		fprintf(stderr, "Too little input: got %d bytes but expected %d.\n", count, incnt);
		exit(3);
	}

	assert(count == incnt);
}


void generate_data(int fd, size_t size)
{
	char buf[BUFSIZ];
	u_int32_t *p;
	u_int32_t *e = (u_int32_t*)(buf + BUFSIZ);
	size_t count = BUFSIZ;

	while(size) {
		if(size < count) {
			// keep e int-aligned.
			e = (u_int32_t*)(buf + (size & ~3) + 4);
			count = size;
		}

		for(p = (u_int32_t*)buf; p < e; p++) {
			// Using the RNG doubles the runtime for large data sets on my
			// system.  118MB takes 0.977 sec w/o, and 1.874 sec with.
			*p = genrand_int32();
			// *p = 0xAABBCCDD;
		}
		size -= count;

		write_buffer(fd, buf, count);
	}
}


int read_num(char *str)
{
	char *remainder;
	int count;

	errno = 0;
	count = strtol(str, &remainder, 10);
	if(errno) {
		printf("Could not recognize \"%s\" as a number.\n", str);
		exit(exit_cmdline_error);
	}
	if(*remainder != '\0') {
		printf("Garbage '%s' in number '%s'\n", remainder, str);
		exit(exit_cmdline_error);
	}

	return count;
}


static void usage()
{
	printf(
			"Usage randfile [OPTION]...\n"
			"  -c --check: check data on stdin instead of generating\n"
			"  -r --seed: specify the randseed (default=1)\n"
			"  -s --size: size in bytes of data to generate (default=1024)\n"
			"  -V --version: print the version of this program.\n"
			"  -h --help: prints this help text.\n"
	);
}


static void process_args(int argc, char **argv)
{
	while(1) {
		int c;
		int optidx = 0; 
		static struct option long_options[] = {
			// name, has_arg (1=reqd,2=opt), flag, val
			{"seed", 1, 0, 'r'},
			{"size", 1, 0, 's'},
			{"check", 0, 0, 'c'},
			{"help", 0, 0, 'h'},
			{"version", 0, 0, 'V'},
			{0, 0, 0, 0},
		};

		c = getopt_long(argc, argv, "hr:s:cV", long_options, &optidx);
		if(c == -1) break;

		switch(c) {
			case 'c':
				opt_check++;
				break;

			case 'r':
				opt_seed = read_num(optarg);
				break;

			case 's':
				opt_size = read_num(optarg);
				break;

			case 'h':
				usage();
				exit(0);

			case 'V':
				printf("randfile version %s\n", VERSION);
				exit(0);
		}
	}

	// but supplying more than one directory is an error.
	if(optind < argc) {
		fprintf(stderr, "Unrecognized arguments: ");
		while(optind < argc) {
			fprintf(stderr, "%s ", argv[optind++]);
		}
		fprintf(stderr, "\n");
		exit(exit_cmdline_error);
	}
}


int main(int argc, char **argv)
{
	if(sizeof(int) != 4) {
		fprintf(stderr, "Your machine's ints are not 4 bytes.  "
				"You need to adjust generate_data() and check_data().\n");
		exit(2);
	}

	process_args(argc, argv);
	init_genrand(opt_seed);

	if(opt_check) {
		check_data(0, opt_size);
	} else {
		generate_data(1, opt_size);
	}

	/*
	int i;
	printf("1000 outputs of genrand_int32()\n");
	for (i=0; i<1000; i++) {
		printf("%10lu ", genrand_int32());
		if (i%5==4) printf("\n");
	}
	*/

	return 0;
}

