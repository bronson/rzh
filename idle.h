/* idle.h
 * 13 June 2005
 * Scott Bronson
 *
 * Displays the continually updated progress.
 */

typedef struct {
	const char *command;	///< the task that this idle proc is watching
	int recv_start_count;	///< number of bytes in the write pipe when the rz started.
	int send_start_count;	///< number of bytes in the read pipe when the rz started.
	int call_cnt;			///< number of times idle proc has been called.
	struct timespec start_time;	///< the time that the transfer started
	struct timespec last_time;	///< the time that the idle proc last updated its display
} idle_state;

idle_state* idle_create(master_pipe *mp, const char *command);
int idle_proc(task_spec *spec);
void idle_end(task_spec *spec);

