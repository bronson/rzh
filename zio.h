/* zio.h
 * Scott Bronson
 * 14 Jan 2004
 *
 * This file is covered by the MIT License.
 */

#include "fifo.h"

extern fifo inma;		// in from master
extern fifo outso;		// stdout
extern fifo insi;		// stdin
extern fifo outma;		// out to master (backchannel)


/* statuses to pass to tick */
#define HAS_ROOM 3
#define HAS_DATA 2
#define IS_EMPTY 1
#define DO_ONCE  0


#define JMP_EOF 1
#define JMP_ERR 2


void zio_bail(fifo *f, char *msg);

typedef void (*bcfn)();
bcfn set_backchannel_fn(bcfn);
void default_backchannel(void);


extern void zio_setup(int ima, int oso, int ini, int oma, jmp_buf *exit);
extern void set_timeout(int sec, int usec);

extern int tick(int code, fifo* buf, int *timeoutp);

extern int get_master(void);	/* longjmps on error */
extern size_t read_master(void *buf, size_t num);	/* longjmps on error */
/* reading insi happens automatically inside tick */
extern void put_stdout(int c);
extern size_t write_master(void *buf, size_t size);

/* in case the master is a modem */
extern void flush_outma();

