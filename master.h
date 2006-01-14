int master_idle();
master_pipe* master_setup(int sockfd);
void master_check_sigchild(master_pipe *mp);
int master_get_window_width(master_pipe *mp);

