#ifndef _TERMINAL_H
#define _TERMINAL_H     1

#include <utils/arguments.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#define CLEAR_SCREEN() printf("\033[2J\033[H")
#define ANSI(c) (get_arguments().no_ansi ? "" : (c))

/*
    GCC ANSI Escape Codes (More Portable)
*/
#if !defined(_WIN32)
#define BLACK     ANSI("\e[0;30m")
#define RED       ANSI("\e[0;31m")
#define GREEN     ANSI("\e[0;32m")
#define YELLOW    ANSI("\e[0;33m")
#define BLUE      ANSI("\e[0;34m")
#define MAGENTA   ANSI("\e[0;35m")
#define CYAN      ANSI("\e[0;36m")
#define WHITE     ANSI("\e[0;37m")

#define BBLACK    ANSI("\e[1;30m")
#define BRED      ANSI("\e[1;31m")
#define BGREEN    ANSI("\e[1;32m")
#define BYELLOW   ANSI("\e[1;33m")
#define BBLUE     ANSI("\e[1;34m")
#define BMAGENTA  ANSI("\e[1;35m")
#define BCYAN     ANSI("\e[1;36m")
#define BWHITE    ANSI("\e[1;37m")
#define RESET     ANSI("\e[0m")
#endif

/*
    MINGW ANSI Escape codes (For Windows CMD)
*/
#if defined(_WIN32)
#define BLACK     ANSI("\x1b[0;30m")
#define RED       ANSI("\x1b[0;31m")
#define GREEN     ANSI("\x1b[0;32m")
#define YELLOW    ANSI("\x1b[0;33m")
#define BLUE      ANSI("\x1b[0;34m")
#define MAGENTA   ANSI("\x1b[0;35m")
#define CYAN      ANSI("\x1b[0;36m")
#define WHITE     ANSI("\x1b[0;37m")

#define BBLACK    ANSI("\x1b[1;30m")
#define BRED      ANSI("\x1b[1;31m")
#define BGREEN    ANSI("\x1b[1;32m")
#define BYELLOW   ANSI("\x1b[1;33m")
#define BBLUE     ANSI("\x1b[1;34m")
#define BMAGENTA  ANSI("\x1b[1;35m")
#define BCYAN     ANSI("\x1b[1;36m")
#define BWHITE    ANSI("\x1b[1;37m")
#define RESET     ANSI("\x1b[0m")
#endif

/*
    Windows as per usual is different as their local codes are very different, they 
    have a code page which while not weird itself theyre formatted differently
    for example "English_United States.65001". This doesnt suck too bad becuase we
    can just check for the .65001 at the end, but for POSIX compliant systems
    we can simply just check for 'utf8' or 'UTF-8'. not sure which one i like more
*/

#endif