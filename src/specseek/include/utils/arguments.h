#ifndef _ARGUMENTS_H
#define _ARGUMENTS_H 1
#include <string.h>

#define _ARGUMENT_VERBOSE                   "--verbose"
#define _ARGUMENT_PUT_FEATURES_ON_NEWLINE   "--features-on-newline"
#define _ARGUMENT_NO_ANSI                   "--no-ansi"
#define _ARGUEMNT_HELP                      "--help"

#define IF_VERBOSE(level) if (get_arguments().verbose >= (level))
#define ARG_MATCH(x) if (strcmp(arg, (x)) == 0)

typedef struct args{
    int verbose;
    int put_features_on_newline;
    int no_ansi;
    int help;
}args_t;

/// @brief changes the argument struct and applies settings based on arguments here
/// @param argc argument count (from main)
/// @param argv argument vector (from main)
/// @param args argument structure reference
int construct_arguments(int argc, const char** argv);

/// @brief 
/// @return 
args_t get_arguments(void);

/// @brief Print the argument help string
int print_help(void);

#endif