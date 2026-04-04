#include <stdlib.h>
#include <utils/common.h>
#include <unistd.h>
#include <utils/arguments.h>
#include <utils/terminal.h>
#include <stdio.h>

static args_t arguments;

/// @brief changes the argument struct and applies settings based on arguments here
/// @param argc argument count (from main)
/// @param argv argument vector (from main)
/// @return Success?
int construct_arguments(int argc, const char** argv) {
    for (int i = 1; i < argc; ++i){
        const char* arg = argv[i];

        // I hate that I have to manually add continue; to all of these
        ARG_MATCH(_ARGUMENT_VERBOSE) { arguments.verbose = (i + 1 < argc && argv[i + 1][0] != '-') ? atoi(argv[++i]) : 1; continue;}
        ARG_MATCH(_ARGUMENT_NO_ANSI) { arguments.no_ansi = 1; continue;}
        ARG_MATCH(_ARGUMENT_PUT_FEATURES_ON_NEWLINE) { arguments.put_features_on_newline = 1; continue;}
        ARG_MATCH(_ARGUEMNT_HELP) {arguments.help = 1; continue;}
        
        printf("%s%s%s is not a valid argument, use --help to see a list of commands, aborting%s\n", BRED, argv[i], RED, RESET);
        return 1;
    }
    
    // TTY check for pipe output purity. we do not want ansi codes there
    // Linux GCC Implementation
    #if defined(__unix__)
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)){
        arguments.no_ansi = 1;
    }
    #endif

    // Windows Compliant Implementation
    #if defined(_WIN32)
    if (!_isatty(STDIN_FILENO) || !_isatty(STDOUT_FILENO)){
        arguments.no_ansi = 1;
    }
    #endif

    return 0;
}

int print_help(void){
    printf("%s%s%s: %s\n", BWHITE, "--verbose {1|2|3}", YELLOW, "show more information, in varing levels, 3 being the most info");
    printf("%s%s%s: %s\n", BWHITE, "--features-on-newline}", YELLOW, "prints all CPU features on a new line for readability");
    printf("%s%s%s: %s\n", BWHITE, "--no-ansi", YELLOW, "inhibits all ANSI escape codes, so no colour or terminal clearing");
    printf("%s%s%s: %s\n", BWHITE, "--help", YELLOW, "prints this help string");
    printf("%s", RESET);        // because for some reason despite the fflush stdiout call it still stains the terminal!!!!
    fflush(stdout);
    return 0;
}

args_t get_arguments(void){
    return arguments;
}

