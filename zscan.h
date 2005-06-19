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


typedef struct {
	void (*found)(struct fifo *f, const char *buf, int size, int fd);
	const char *ref;	// remembers where in the zfin string we're scanning.
	int oocount;		// remembers how many Os we've seen

	char *savebuf;	// saves all data after the ZFIN+OO.
	int savemax;
	int savecnt;
} zfinscanstate;


zscanstate* zscan_create(zstart_proc proc, void *refcon);
void zscan_destroy(zscanstate *state);
void zscan(zscanstate *conn, const char *cb, const char *ce, struct fifo *f, int fd);

zfinscanstate* zfinscan_create(void (*proc)(struct fifo *f, const char *buf, int size, int fd));
void zfinscan_destroy(zfinscanstate *state);
void zfinscan(struct fifo *f, const char *buf, int size, int fd);
void zfinnooo(struct fifo *f, const char *buf, int size, int fd);
void zfinsave(struct fifo *f, const char *buf, int size, int fd);
void zfindrop(struct fifo *f, const char *buf, int size, int fd);

