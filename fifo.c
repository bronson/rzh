/* fifo.c
 * Scott Bronson
 * 14 Jan 2004
 *
 * This file is contributed to the public domain.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "fifo.h"


/* name is an arbitrary name for the fifo */
fifo *fifo_init(char *name, fifo *f, int initsize)
{
	f->name = name;
	f->len = initsize;
	f->beg = f->end = 0;
	f->error = 0;
	f->fd = 0;
	f->state = FIFO_IN_USE;

	f->buf = (char*)malloc(initsize);
	if(f->buf == NULL) return NULL;

	return f;
}


/* erase all data in the fifo */
void fifo_clear(fifo *f)
{
	f->beg = f->end = 0;
}


/* bytes in the fifo */
int fifo_count(fifo *f)
{
	return (f->len + f->end - f->beg) % f->len;
}


/* free space in the fifo */
int fifo_avail(fifo *f)
{
	return (f->len + f->beg - f->end - 1) % f->len;
}


/* add a character to the fifo */
/* make sure there's room before calling! */
void fifo_unsafe_addchar(fifo *f, char c)
{
	f->buf[f->end++] = c;
	if(f->end == f->len) f->end = 0;
}


int fifo_unsafe_getchar(fifo *f)
{
	int c = f->buf[f->beg++];
	if(f->beg == f->len) f->beg = 0;
	return c;
}


/* add a block of data to the fifo */
/* make sure there's room before calling! */
void fifo_unsafe_append(fifo *f, const char *buf, int cnt)
{
	if(f->end + cnt > f->len) {
		int n = f->len - f->end;
		memcpy(f->buf+f->end, buf, n);
		memcpy(f->buf, buf+n, cnt - n);
	} else {
		memcpy(f->buf+f->end, buf, cnt);
	}

	f->end = (f->end + cnt) % f->len;
}


void fifo_unsafe_prepend(fifo *f, const char *buf, int cnt)
{
	if(f->beg < cnt) {
		int n = cnt - f->beg;
		memcpy(f->buf, buf + n, f->beg);
		memcpy(f->buf + f->len - n, buf, n);
		f->beg = f->len - n;
	} else {
		f->beg -= cnt;
		memcpy(f->buf + f->beg, buf, cnt);
	}
}


void fifo_unsafe_unpend(fifo *f, char *buf, int cnt)
{
	if(f->beg + cnt > f->len) {
		int n = f->len - f->beg;
		memcpy(buf, f->buf+f->beg, n);
		memcpy(buf+n, f->buf, cnt - n);
	} else {
		memcpy(buf, f->buf+f->beg, cnt);
	}

	f->beg = (f->beg + cnt) % f->len;
}

/*
static void print_fifo(fifo *f)
{
	printf("fifo at %08lX  ", (long)f);
	printf("%s  ", f->name);
	printf("beg=%d end=%d len=%d", (int)f->beg, (int)f->end, (int)f->len);
	printf("  count=%d avail=%d\r\n", (int)fifo_count(f), (int)fifo_avail(f));
}
*/


/* partially fill the fifo by calling read() */
/* only performs a single read call. */

int fifo_read(fifo *f, int fd)
{
	char buf[BUFSIZ];
	int cnt;

	cnt = fifo_avail(f);
	if(cnt > sizeof(buf)) {
		cnt = sizeof(buf);
	}

	cnt = read(fd, buf, cnt);
	if(cnt > 0) {
		fifo_unsafe_append(f, buf, cnt);
	} else if(cnt < 0) {
		if(errno == EAGAIN) {
			cnt = 0;
		} else {
			f->error = (errno ? errno : -1);
		}
	}

	return cnt;
}


/* attempt to empty the fifo by calling write() */
int fifo_write(fifo *f, int fd)
{
	int cnt = 0;

	if(f->beg < f->end) {
		cnt = write(fd, f->buf+f->beg, f->end-f->beg);
		if(cnt < 0) f->error = (errno ? errno : -1);
		if(cnt > 0) f->beg += cnt;
	} else if(f->beg > f->end) {
		cnt = write(fd, f->buf+f->beg, f->len-f->beg);
		if(cnt < 0) f->error = (errno ? errno : -1);
		if(cnt > 0) {
			f->beg = (f->beg + cnt) % f->len;
			if(f->beg == 0) {
				int n = write(fd, f->buf, f->end);
				if(n < 0) { cnt = n; f->error = (errno ? errno : -1); }
				if(n > 0) { f->beg = n; cnt += n; }
			}
		}
	}

	return cnt;
}


/* copies as much of the contents of one fifo as possible
 * to the other */

int fifo_copy(fifo *src, fifo *dst)
{
	int cnt = fifo_count(src);
	int ava = fifo_avail(dst);
	if(ava < cnt) cnt = ava;

	if(src->beg + cnt > src->len) {
		int n = src->len - src->beg;
		fifo_unsafe_append(dst, src->buf+src->beg, n);
		fifo_unsafe_append(dst, src->buf, cnt - n);
	} else {
		fifo_unsafe_append(dst, src->buf+src->beg, cnt);
	}

	src->beg = (src->beg + cnt) % src->len;
	return cnt;
}

void fifo_flush(fifo *f)
{
#ifdef TCFLSH
	ioctl(f->fd, TCFLSH, 0);
#else       
	lseek(f->fd, 0L, 2);
#endif
}
