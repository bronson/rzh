extern pipe_atom a_master;
extern pipe_atom a_stdin;
extern pipe_atom a_stdout;

extern struct pipe p_input_master;
extern struct pipe p_master_output;

extern int g_slave_fd;

void start_proc(const char *buf, int size, void *refcon);
