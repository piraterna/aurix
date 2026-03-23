#ifndef SH_PARSE_H
#define SH_PARSE_H

int parse_line(char *line, char *argv[], int max_args);
char *expand_env_token(const char *input);
int split_pipeline(char *line, char *segments[], int max_segments);
int split_and_list(char *line, char *segments[], int max_segments);

#endif
