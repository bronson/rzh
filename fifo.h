/* fifo.h
 * Scott Bronson
 * 14 Jan 2004
 *
 * This file is contributed to the public domain.
 */


typedef struct {
	char *name;
	char *buf;
	int beg, end, len;	/* must be signed */
	int fd, error;		/* used by clients */
	int state;
} fifo;


/* fifo states */
#define FIFO_UNUSED 0
#define FIFO_IN_USE 1
#define FIFO_CLOSED 2


#define fifo_empty(f) 		((f)->beg == (f)->end)


/* allocates a fifo initialially able to hold initsize chars
 * and will grow to hold maxsize chars if needed.
 * Returns NULL if fifo memory couldn't be allocated.
 */
fifo* fifo_init(char *name, fifo *f, int initsize);

void fifo_clear(fifo *f);      /* empty the fifo of all data */
int fifo_count(fifo *f);    /* number of bytes of data in the fifo */
int fifo_avail(fifo *f);    /* free bytes left in the fifo */

void fifo_unsafe_addchar(fifo *f, char c);
int fifo_unsafe_getchar(fifo *f);

/* stuff a memory block into the fifo */
void fifo_unsafe_append(fifo *f, const char *buf, int cnt);
void fifo_unsafe_prepend(fifo *f, const char *buf, int cnt);
/* grab a memory block out of the fifo */
void fifo_unsafe_unpend(fifo *f, char *buf, int cnt);

/* fill the fifo by calling read() */
int fifo_read(fifo *f, int fd);
/* empty the fifo by calling write() */
int fifo_write(fifo *f, int fd);
/* copy as much of the data from src as will fit into dst */
int fifo_copy(fifo *src, fifo *dst);

/* try to flush the fifo to its fd */
void fifo_flush(fifo *f);

