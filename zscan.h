/* zscan.h
 * Scott Bronson
 * 13 June 2005
 */


typedef void (*zstart_proc)(void *refcon);


typedef struct {
	int parse_state;
	enum { RZNONE, RZNL, RZCR, RZCRNL } gotrz;
	int starcnt;

	zstart_proc start_proc;
	void *start_refcon;
} zscanstate;


zscanstate* zscan_create(zstart_proc proc, void *refcon);
void zscan_destroy(zscanstate *state);

void zscan(zscanstate *conn, const char *cb, const char *ce, struct fifo *f, int fd);
void zfinscan(struct fifo *f, const char *buf, int size, int fd);
