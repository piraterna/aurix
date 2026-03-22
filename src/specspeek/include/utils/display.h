#pragma once

#include <stdio.h>
#include <utils/terminal.h>

#define TERM_BANNER(title, colour) \
    do { \
        printf("%s", colour); \
        printf("==================================\n"); \
        printf("   %s                        \n", title); \
        printf("==================================\n"); \
        printf("%s", RESET); \
    } while(0)

#define TERM_DIVIDER(title, colour) \
    do { \
        printf("%s", colour); \
        printf("\n========== %s ==========\n", title); \
        printf("%s", RESET); \
    } while(0)

#define TERM_DIVIDER_NOTEXT(colour) \
    do { \
        printf("%s", colour); \
        printf("\n==============================================\n"); \
        printf("%s", RESET); \
    } while(0)

#define TERM_DIVIDER_SMALL(title, colour) \
    do { \
        printf("%s", colour); \
        printf("\n~~~~~~~~~~~ %s ~~~~~~~~~~\n", title); \
        printf("%s", RESET); \
    } while(0)

#define TERM_DIVIDER_SMALL_NOTEXT(colour) \
    do { \
        printf("%s", colour); \
        printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"); \
        printf("%s", RESET); \
    } while(0)

#define SYMBOL_LIST_CHILD(colour) "\\"

#define SYMBOL_LIST_END(colour) "\\"