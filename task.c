/* task.c
 * 14 June 2005
 * Scott Bronson
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include "log.h"
#include "fifo.h"
#include "io/io.h"
#include "pipe.h"
#include "task.h"
#include "util.h"


/** This uses the spec to set up all the memory and atoms
 *  needed by the task.  It doesn't actually install the task.
 */

static task_state* task_prepare(task_spec *spec)
{
	task_state *task;
	int err;

	task = malloc(sizeof(task_state));
	if(task == NULL) {
		perror("allocating task");
		bail(42);
	}

	if(spec->infd >= 0) {
		pipe_atom_init(&task->read_atom, spec->infd);
	} else {
		task->read_atom.atom.fd = -1;
	}

	if(spec->outfd >= 0) {
		pipe_atom_init(&task->write_atom, spec->outfd);
	} else {
		task->write_atom.atom.fd = -1;
	}

	if(spec->errfd >= 0 && spec->err_proc) {
		task->err_atom.refcon = spec->err_refcon;
		io_atom_init(&task->err_atom.atom, spec->errfd, spec->err_proc);
		err = io_add(&task->err_atom.atom, IO_READ);
		if(err != 0) {
			fprintf(stderr, "%d (%s) setting up err atom for fd %d",
					err, strerror(-err), spec->errfd);
			bail(72);
		}
	} else {
		task->err_atom.atom.fd = -1;
	}

	task->next = NULL;
	task->spec = spec;

	return task;
}


static void task_destroy(task_state *task, int free_mem)
{
	if(task->read_atom.atom.fd >= 0) {
		pipe_atom_destroy(&task->read_atom);
	}
	if(task->write_atom.atom.fd >= 0) {
		pipe_atom_destroy(&task->write_atom);
	}
	if(task->err_atom.atom.fd >= 0) {
		io_del(&task->err_atom.atom);
	}

	(*task->spec->destruct_proc)(task->spec, free_mem);

	if(free_mem) {
		free(task);
	}
}


/** Inserts the topmost task on the pipe into the master pipe.
 *  Used to insert a new task or to restore an old one.
 *  NOTE: do NOT use the old state to manipulate data structures.
 *  Write to them only, do not read.
 */

static void task_pipe_setup(master_pipe *mp)
{
	task_state *task = mp->task_head;

	// Handle verso first.  We need to restore the read proc on this task
	// to its original state (eradicate any verso from a subtask).  This
	// means that read atoms in normal usage MUST be pipe endpoints.
	// Seems an OK restriction to me.
	if(task->read_atom.atom.fd >= 0) {
		task->read_atom.atom.proc = pipe_io_proc;
	}

	// Now, if the task wants verso read, install it.
	if(task->spec->verso_input_proc && task->next) {
		task_state *verso = task->next;
		if(verso->read_atom.atom.fd >= 0) {
			verso->read_atom.atom.proc = task->spec->verso_input_proc;
			verso->read_atom.read_pipe = task->spec->verso_input_refcon;
			// ensure that reading is enabled
			io_enable(&verso->read_atom.atom, IO_READ);
		}
	}

	// Splice the atoms into the pipes
	mp->input_master.read_atom = &task->read_atom;
	task->read_atom.read_pipe = &mp->input_master;
	mp->master_output.write_atom = &task->write_atom;
	task->write_atom.write_pipe = &mp->master_output;

	// New reader so reset the read status
	mp->input_master.block_read = 0;
	if(mp->input_master.read_atom->atom.fd >= 0) {
		io_enable(&mp->input_master.read_atom->atom, IO_READ);
	}

	// Ensure the fifo procs are set up
	mp->input_master.fifo.proc = task->spec->inma_proc;
	mp->input_master.fifo.refcon = task->spec->inma_refcon;
	mp->master_output.fifo.proc = task->spec->maout_proc;
	mp->master_output.fifo.refcon = task->spec->maout_refcon;
}


/** Installs the given task as the topmost task.
 */

void task_install(master_pipe *mp, task_spec *spec)
{
	task_state *task = task_prepare(spec);

	// insert into the linked list
	task->next = mp->task_head;
	mp->task_head = task;
	task->spec->master = mp;

	task_pipe_setup(mp);
}


/** Removes and disposes of the topmost task_state.
 *  The task_spec's destructor is also called if it's provided.
 */

void task_remove(master_pipe *mp)
{
	task_state *task = mp->task_head;
	
	// remove from the linked list
	mp->task_head = task->next;

	if(mp->task_head) {
		// restore the prevous task in the pipe
		task_pipe_setup(mp);
		task_destroy(task, 1);
	} else {
		// no more tasks, call the pipe destructor
		task_destroy(task, 1);
		(*mp->destruct_proc)(mp, 1);
	}

	task->spec->master = NULL;
}


/** Closes all open filehandles and frees the memory used by the
 *  task spec.  If you don't want to close any of the filehandles,
 *  set them to -1 in your destructor before calling this one.
 */

void task_default_destructor(task_spec *spec, int free_mem)
{
	if(spec->infd >= 0) close(spec->infd);
	if(spec->outfd >= 0) close(spec->outfd);
	if(spec->errfd >= 0) close(spec->errfd);

	// We'll do nothing with the child pid.

	if(free_mem) {
		free(spec);
	}
}


/** Given a task_spec, returns the associated task_state.
 *  What a stupid function to have!  Things should be reorg'd so there's
 *  no need to ever go from a spec back to the state.
 */

static task_state* master_pipe_find_task(master_pipe *mp, task_spec *spec)
{
	task_state *task = mp->task_head;

	while(task) {
		if(task->spec == spec) {
			return task;
		}
	}

	return NULL;
}


void task_default_sigchild(master_pipe *mp, task_spec *spec, int pid)
{
	task_state *task;

	if(pid != spec->child_pid) {
		// nothing to do if it's not our child.
		return;
	}

	log_dbg("handling SIGCHLD for pid %d", pid);

	task = master_pipe_find_task(mp, spec);
	assert(task);
	assert(task->spec == spec);

	while(task && task->read_atom.atom.fd != -1) {
		// We got a sigchld for this task, but the reader hasn't
		// been closed yet.  This means there's probably a touch
		// more data in the read pipe.  Read it to exhaustion.
		while(task->read_atom.atom.fd != -1) {
			log_info("Found extra data in pipe %d:", task->read_atom.atom.fd);
			pipe_io_proc(&task->read_atom.atom, IO_READ);
		}
		if(mp->input_master.write_atom->atom.fd >= 0) {
			log_info("Wrote extra data to %d\n", mp->input_master.write_atom->atom.fd);
			fifo_write(&mp->input_master.fifo, mp->input_master.write_atom->atom.fd);
		}
	}

	if(spec == mp->task_head->spec) {
		// And it's topmost so just remove it
		if(!fifo_empty(&mp->input_master.fifo)) {
			log_info("input->master fifo still had %d bytes!", fifo_count(&mp->input_master.fifo));
		}
		if(!fifo_empty(&mp->master_output.fifo)) {
			log_info("master->output fifo still had %d bytes!", fifo_count(&mp->master_output.fifo));
		}

		task_remove(mp);
	} else {
		// We have a problem.  A task deep in our chain bailed.
		// Since, for now, this can only mean that the echo shell
		// just exited, which means that the master pipe is gone
		// anyway.  Nothing we can do except bail.  This must change
		// if deeper pipes are being set up.
		log_err("Parent process exited unexpectedly!\n");
		fprintf(stderr, "Parent process exited unexpectedly!\n");
		bail(43);
	}
}


/** This is expected to terminate your task.  It needn't clean anything
 *  up.  That will be taken care of when your task is removed.
 *
 *  Default behavior is just to kill the child.  We'll get a SIGCHLD
 *  which will tell us to clean everything up.
 */

void task_default_terminate(master_pipe *mp, task_spec *spec)
{
	if(spec->child_pid > 0) {
		kill(spec->child_pid, SIGTERM);
	}
}


/** Sets up a new task_spec, ready to be filled in.
 *  The destructor is guaranteed to be set to the default
 *  destructor, task_destructor().
 */

task_spec* task_create_spec()
{
	// calloc is giving me trouble; dunno why.
	task_spec *spec = malloc(sizeof(task_spec));
	if(spec == NULL) {
		return NULL;
	}
	memset(spec, 0, sizeof(task_spec));

	spec->infd = -1;
	spec->outfd = -1;
	spec->errfd = -1;
	spec->child_pid = -1;

	spec->destruct_proc = task_default_destructor;
	spec->sigchild_proc = task_default_sigchild;
	spec->terminate_proc = task_default_terminate;

	return spec;
}


/** Call this after you fork a child but before you exec.
 *  It cleans things up for the child, mostly closing unneeded fds.
 *  (does this by calling all destructors with a free_mem of 0)
 */

void task_fork_prepare(master_pipe *mp)
{
	task_state *task = mp->task_head;

	while(task) {
		task_destroy(task, 0);
		task = task->next;
	}

	(*mp->destruct_proc)(mp, 0);
}


/** This calls each sigchild handler when we receive a SIGCHLD,
 *  regardless of pid.
 */

void task_dispatch_sigchild(master_pipe *mp, int pid)
{
	task_state *task = mp->task_head;
	task_state *ntask;

	while(task) {
		// since the task may be disposed when we call its sigchild_proc,
		// we need to copy everything we need first.
		ntask = task->next;
		(*task->spec->sigchild_proc)(mp, task->spec, pid);
		task = ntask;
	}

	(*mp->sigchild_proc)(mp, pid);
}


/** The default destructor for master pipes.
 *  free_mem is set to 0 if we're forking, or 1 if we're quitting.
 *  No need to free mem before forking since everything will be
 *  initialized anyway.  Want to free mem before quitting though
 *  to try to discover memory leaks.
 */

void master_pipe_default_destructor(master_pipe *mp, int free_mem)
{
	pipe_atom_destroy(&mp->master_atom);

	if(free_mem) {
		pipe_destroy(&mp->input_master);
		pipe_destroy(&mp->master_output);
		free(mp);
	}
}


void master_pipe_default_sigchild(master_pipe *mp, int pid)
{
	// do nothing
}


void master_pipe_default_terminate(master_pipe *mp)
{
	// do nothing
}


/** For now the master must read and write the same fd.
 */

master_pipe* master_pipe_init(int masterfd)
{
	master_pipe *mp = malloc(sizeof(master_pipe));
	if(mp == NULL) {
		return NULL;
	}
	memset(mp, 0, sizeof(master_pipe));

	pipe_atom_init(&mp->master_atom, masterfd);

	pipe_init(&mp->input_master, NULL, &mp->master_atom, 8192);
	pipe_init(&mp->master_output, &mp->master_atom, NULL, 8192);

	mp->destruct_proc = master_pipe_default_destructor;
	mp->sigchild_proc = master_pipe_default_sigchild;
	mp->terminate_proc = master_pipe_default_terminate;

	mp->task_head = NULL;

	return mp;
}


void master_pipe_terminate(master_pipe *mp)
{
	task_state *task = mp->task_head;

	while(task) {
		(*task->spec->terminate_proc)(mp, task->spec);
		task = task->next;
	}

	(*mp->terminate_proc)(mp);

	// TODO: Should probably wait around for kids to die...?

	while(mp->task_head) {
		task_remove(mp);
	}

	// now the pipe should be entirely destroyed.
}


