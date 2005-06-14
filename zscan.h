/* zscan.h
 * Scott Bronson
 * 13 June 2005
 */


typedef void (*zstart_proc)(const char *buf, int size, void *refcon);

typedef struct {
	int parse_state;
	enum { RZNONE, RZNL, RZCR, RZCRNL } gotrz;
	int starcnt;

	zstart_proc start_proc;
	void *start_refcon;
} zscanstate;


void zscanstate_init(zscanstate *conn);
void zscan(zscanstate *conn, const char *cb, const char *ce, struct fifo *f);

