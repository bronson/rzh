int master_idle();
master_pipe* master_setup(int sockfd);
void master_check_sigchild(master_pipe *mp);

