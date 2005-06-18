/* task.h
 * 14 June 2005
 * Scott Bronson
 *
 * A task is a proces to splice into the master pipe.  For instance,
 * when we fork rzh, we're creating a task to insert.  This is a way
 * of trying to keep the complexity manageable.  You should have seen
 * this code before it was organized into tasks and pipes!
 */


struct master_pipe;


/** The information required to set up a task.  This is what the task
 *  sets up to hand to task_install.
 */

typedef struct task_spec {
	int infd;			///< fd that provides data to the master (i.e. child's stdout)
	int outfd;			///< fd that receives data from the master (child's stdin)
	int errfd;			///< for now any data received just goes into the debug log.
	int child_pid;		///< pid of child forked or -1.

	fifo_proc inma_proc;	///< proc to process the data flowing from input to master.  Note that the read proc is passed all errors too.  -2 indicates EOF, -1 indicates an error (check errno for exactly what error).  So, if cnt>1 you have data to process, else you don't.  You should probably never receive cnt=0 but I wouldn't rely on this.
	void *inma_refcon;		///< and the refcon to pass to it.
	fifo_proc maout_proc;	///< proc to process the data flowing from master to output.
	void *maout_refcon;		///< and the refcon to pass to it.

	io_proc err_proc;		///< This routine is called every time something arrives on stderr.
	void *err_refcon;
	io_proc verso_input_proc;	///< The verso_input is the input that was disconnected so that the new input could be attached to the master.  This proc is called whever data is available on the verso input.
	void *verso_input_refcon;
	// Turns out we don't need a verso output proc...?
	// Right now, verso output is a complete hack.  It works though.
	// The entire pipe_atom is available for use by the verso read proc, since it gets entirely reset by task_pipe_setup.  Cool!

/** The idle proc of the topmost task gets called once each time through
 *  the event loop.  It returns the number of milliseconds that should
 *  elapse before being called again.  Note that it is called EVERY time
 *  through the event loop so it will probably get called much more
 *  often than it requests.
 */
	int (*idle_proc)(struct task_spec*);			///< The idle proc to call when this task is topmost.
	void (*destruct_proc)(struct task_spec*, int forking);	///< Called when the task gets removed so the task_spec is no longer needed (unless you want to reuse it of course).  This routine is to free all memory, etc.  If forking is true, then we're running in a child that is about to exec, so close all filehandles but don't worry about memory.
	void (*sigchild_proc)(struct master_pipe*, struct task_spec*, int pid);	///< The SIGCHLD handler for this task.  See task_sigchild() for more.  This is set to task_default_sigchild by default.  If your task doesn't involve forked children, just leave task_spec::child_pid set to -1.
	void (*terminate_proc)(struct master_pipe*, struct task_spec*);

	struct master_pipe *master;
	void *refcon;
} task_spec;


/** The state required to run the task.
 */

typedef struct {
	io_atom atom;
	void *refcon;
} heavy_atom;

typedef struct task_state {
	pipe_atom read_atom;		///< The input (input -> master)
	pipe_atom write_atom;		///< The output (master -> output)
	heavy_atom  err_atom;			///< The error (a proc is called but no pipes are provided)
	struct task_state *next;	///< The next task downward.  When this task is removed, the next task will take over.
	task_spec *spec;			///< The spec that this task was created from.
} task_state;


/** This is what tasks are inserted into: the master pipe.
 *  It is bidirectional, input->master, and master->output.
 *  The destructor is called when the last task is removed.
 */

typedef struct master_pipe {
	pipe_atom master_atom;		///< The fd of the master, both read and write.
	struct pipe input_master;	///< The pipe shuttling data from the input to the master.
	struct pipe master_output;	///< The pipe shuttling data from the master to the output.
	void (*destruct_proc)(struct master_pipe*, int free_mem);	///< Called when the last task is removed from the pipe.
	void (*sigchild_proc)(struct master_pipe*, int pid);	///< The SIGCHLD handler for this task.  See task_sigchild() for more.  This is set to task_default_sigchild by default.  If your task doesn't involve forked children, just leave task_spec::child_pid set to -1.
	void (*terminate_proc)(struct master_pipe*);
	task_state *task_head;		///< the tasks in order from topmost to bottommost.
	void *refcon;				///< Used for whatever the pipe wants (not used by task code).
} master_pipe;


void task_install(master_pipe *mp, task_spec *spec);
void task_remove();

// used when writing tasks
task_spec* task_create_spec();
void task_default_destructor(task_spec *spec, int free_mem);

void task_dispatch_sigchild(master_pipe *mp, int pid);
void task_fork_prepare(master_pipe *mp);

master_pipe* master_pipe_init(int masterfd);
void master_pipe_default_destructor(master_pipe *mp, int free_mem);
void master_pipe_terminate(master_pipe *mp);

