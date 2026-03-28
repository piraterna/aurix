#ifndef SH_HISTORY_H
#define SH_HISTORY_H

void sh_history_add(const char *line);
const char *sh_history_prev(const char *current);
const char *sh_history_next(void);

#endif
