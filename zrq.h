/* zrq.h
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


zscanstate* zrq_create(zstart_proc proc, void *refcon);
void zrq_destroy(zscanstate *state);
void zrq_scan(zscanstate *conn, const char *cb, const char *ce, struct fifo *f, int fd);

