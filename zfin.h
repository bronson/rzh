/* zfin.h
 * Scott Bronson
 * 13 June 2005
 */


typedef struct {
	void (*found)(struct fifo *f, const char *buf, int size, int fd);
	const char *ref;	// remembers where in the zfin string we're scanning.
	int oocount;		// remembers how many Os we've seen

	char *savebuf;	// saves all data after the ZFIN+OO.
	int savemax;
	int savecnt;

	master_pipe *master;
} zfinscanstate;


zfinscanstate* zfin_create(master_pipe *mp,
		void (*proc)(struct fifo *f, const char *buf, int size, int fd));
void zfin_destroy(zfinscanstate *state);
void zfin_scan(struct fifo *f, const char *buf, int size, int fd);
void zfin_nooo(struct fifo *f, const char *buf, int size, int fd);
void zfin_save(struct fifo *f, const char *buf, int size, int fd);
void zfin_term(struct fifo *f, const char *buf, int size, int fd);
void zfin_drop(struct fifo *f, const char *buf, int size, int fd);

