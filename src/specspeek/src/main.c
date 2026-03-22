#include <stdio.h>
#include <main.h>
#include <utils/display.h>
#include <utils/terminal.h>
#include <output/cpu_info.h>
#include <utils/arguments.h>
#include <system/hardware/CPU/cpu.h>
#include <system/hardware/CPU/intel/processors.h>
#include <system/hardware/CPU/microarch.h>
#include <system/hardware/CPU/specifications.h>
#include <system/hardware/CPU/virtualisation.h>

/// @brief Spec Seek Entry Point
/// @param argc argument count
/// @param argv argument vector
int main(int argc, const char** argv){
    if (construct_arguments(argc, argv) != 0) fprintf(stderr, "%sFailure in Constructing Arguments", RED);
    if (!get_arguments().no_ansi) CLEAR_SCREEN();
    if (get_arguments().help){ print_help(); return 0; }
    
    TERM_DIVIDER("Spec Seek by Mellurboo", BLUE);
    printf("%sCompiled with GCC Version %d.%d.%d\n", BLUE,__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    printf("%sCompiled at: %s @ %s", BLUE, __DATE__, __TIME__);
    TERM_DIVIDER_NOTEXT(BLUE);

    print_cpu_info(init_cpu());

    return exit_specseek();
}

/// @brief specseek has finished and probably without any issues, any logic that happens before we finish goes here.
int exit_specseek(){
    // Windows will close straight away after execution so this allows users to read it.
    #if defined(_WIN32)
    printf("\n%sSpec Seek has finished, Press any key to exit: ", BWHITE);
    getchar();
    #endif

    // This will stop us from staining the terminal session
    printf("%s", RESET);
    fflush(stdout);
    return 0;
}