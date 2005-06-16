extern int g_highest_fd;

void fdcheck();
int find_highest_fd();

// provided by rzh.
extern void bail(int val);
void rzh_fork_prepare();

// result codes returned by exit().
// actually, these are mostly used by main, not bgio

enum {
	argument_error=1,
	chdir_error=2,
	fork_error1=5,
	fork_error2=6,
	fork_error3=7,
	process_error=10,
};
