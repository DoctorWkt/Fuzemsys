/* syscalls.c */
void set_fuzix_root(char *dirname);
int set_arg_env(uint16_t sp, char **argv, char **envp);
void set_initial_brk(uint16_t addr);
int do_syscall(int op, int *longresult);
