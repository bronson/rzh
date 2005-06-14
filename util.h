extern int g_highest_fd;

extern int rz_child_pid;
extern int bgio_child_pid;

extern void bail();
extern void stop_rz_child();
extern void stop_bgio_child();

void sigchild(int tt);
void fdcheck();
int find_highest_fd();

