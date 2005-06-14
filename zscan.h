/* zscan.h
 * Scott Bronson
 * 13 June 2005
 */


typedef struct {
	int parse_state;
	enum { RZNONE, RZNL, RZCR, RZCRNL } gotrz;
	int starcnt;
} zscanstate;


void zscanstate_init(zscanstate *conn);
void zscan(zscanstate *conn, const char *cb, const char *ce, fifo *f);

