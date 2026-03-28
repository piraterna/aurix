#ifndef SH_EXEC_H
#define SH_EXEC_H

#include <stdbool.h>

int run_command_line(char *line, int *last_status, bool *should_exit);
void run_shinit(int *last_status, bool *should_exit);

#endif
