#include <stdio.h>
#include <utils/display.h>
#include <utils/terminal.h>
#include <utils/arguments.h>
#include <output/cpu_info.h>
#include <system/hardware/CPU/intel/processors.h>
#include <system/hardware/CPU/specifications.h>

/// @brief Prints the processors supported Standard and Extended Leaves
/// @param cpu_ctx cpu context
/// @param min_verbosity verbosity required to print
void print_cpu_cpuid_supported_leafs(cpu_t cpu_ctx, int min_verbosity){
    IF_VERBOSE(min_verbosity){
        TERM_DIVIDER("CPUID", BOLD_VENDOR_COLOUR);
        printf("%sMax Supported CPUID Standard Leaf: %s0x%X\n", BWHITE, VENDOR_COLOUR, cpu_get_max_supported_leaf());
        printf("%sMax Supported CPUID Extended Leaf: %s0x%X", BWHITE, VENDOR_COLOUR, cpu_get_max_supported_extended_leaf());
        TERM_DIVIDER_NOTEXT(BOLD_VENDOR_COLOUR);
    }
}

/// @brief Prints all identifying information about the processor
/// @param cpu_ctx cpu context
void print_cpu_processor_identifiers(cpu_t cpu_ctx){
    TERM_DIVIDER("Processor", BOLD_VENDOR_COLOUR);
    printf("%sName:      %-8s%s\n", BWHITE, VENDOR_COLOUR, cpu_ctx.name);

    printf("%sMicroarch: %-8s%s\n", BWHITE, 
        VENDOR_COLOUR,
        cpu_get_microarch(cpu_ctx.family, cpu_ctx.model, cpu_ctx.ext_model, cpu_ctx.revision)
    );

    printf("%sVendor:    %-8s%s\n", BWHITE, VENDOR_COLOUR, cpu_ctx.vendor);
    TERM_DIVIDER_SMALL("identifiers", BOLD_VENDOR_COLOUR);
    printf("%sModel: %-10s0x%X\n", BWHITE, VENDOR_COLOUR, cpu_ctx.model);
    printf(" %s%sBase Model:%-9s0x%X\n", SYMBOL_LIST_CHILD(VENDOR_COLOUR), BWHITE, VENDOR_COLOUR, cpu_ctx.base_model);
    printf(" %s%sExt. Model:%-9s0x%X\n", SYMBOL_LIST_END(VENDOR_COLOUR), BWHITE, VENDOR_COLOUR, cpu_ctx.ext_model);
    printf("%sFamily: %-9s0x%X\n", BWHITE, VENDOR_COLOUR, cpu_ctx.family);
    printf(" %s%sBase Family:%-8s0x%X\n", SYMBOL_LIST_CHILD(VENDOR_COLOUR), BWHITE, VENDOR_COLOUR, cpu_ctx.base_family);
    printf(" %s%sExt. Family:%-8s0x%X\n", SYMBOL_LIST_END(VENDOR_COLOUR), BWHITE, VENDOR_COLOUR, cpu_ctx.ext_family);
    printf("%sRevision:%-8s0x%X", BWHITE, VENDOR_COLOUR, cpu_ctx.revision);
}

/// @brief Prints all CPU specification Information
/// @param cpu_ctx cpu context
void print_cpu_specification(cpu_t cpu_ctx){
    TERM_DIVIDER_SMALL("specifications", BOLD_VENDOR_COLOUR);
    printf("%sLogical Processors: %-8s%u\n", BWHITE, VENDOR_COLOUR, cpu_ctx.logical_processors);
    printf(" %s%sPhysical Cores:  %-9s%u\n", SYMBOL_LIST_CHILD(VENDOR_COLOUR), BWHITE, VENDOR_COLOUR, cpu_ctx.physical_processors);
    printf(" %s%sThreads Per Core: %-8s%u\n", SYMBOL_LIST_END(VENDOR_COLOUR), BWHITE, VENDOR_COLOUR, cpu_ctx.thread_count_per_core);
    
    IF_VENDOR_INTEL({
        if (cpu_supports_standard_leaf(0x0000001A)){
            printf("  %sPerformance Cores: %s%u\n", BWHITE, VENDOR_COLOUR, cpu_ctx.performance_cores);
            printf("  %sEfficiency Cores: %s%u\n", BWHITE, VENDOR_COLOUR, cpu_ctx.efficient_cores);
        }
    });
}

/// @brief Prints standard, extended and misc features for the CPU
/// @param cpu_ctx cpu context
void print_cpu_features(cpu_t cpu_ctx){
    TERM_DIVIDER("CPUs Feature Support", BYELLOW);
    TERM_DIVIDER_SMALL("Standard Features", BYELLOW);
    reset_cpu_feature_count();
    cpu_check_standard_features();

    TERM_DIVIDER_SMALL("Extended Features", BYELLOW);
    reset_cpu_feature_count();
    cpu_check_extended_features();

    TERM_DIVIDER_SMALL(" Extra. Features ", BYELLOW);
    reset_cpu_feature_count();
    cpu_check_misc_features();
    TERM_DIVIDER_NOTEXT(BYELLOW);
}

void print_cpu_cache_info(cpu_cache_info_t cache){
    TERM_DIVIDER_SMALL("  Cache Info  ", VENDOR_COLOUR);
    printf("%sLevel 3: %s%ukB\n", WHITE, VENDOR_COLOUR, cache.l3);
    printf(" %s%sLevel 2: %s%ukB\n", SYMBOL_LIST_CHILD(VENDOR_COLOUR), WHITE, VENDOR_COLOUR,  cache.l2);
    printf("  %s%sLevel 1: %s%ukB\n", SYMBOL_LIST_CHILD(VENDOR_COLOUR), WHITE, VENDOR_COLOUR,  cache.l1d);
    printf("   %s%sLevel 1i: %s%ukB\n", SYMBOL_LIST_END(VENDOR_COLOUR), WHITE, VENDOR_COLOUR,  cache.l1i);
}

void print_cpu_clock_info(){
    TERM_DIVIDER_SMALL("  Crystal Clock  ", VENDOR_COLOUR);
    printf("%sCrystal Clock Frequency = %s%uhz\n", WHITE, BWHITE, cpu_get_nominal_frequency());
}

/// @brief calls upon all cpu print functions, does not override min_verbosity
/// @param cpu_ctx cpu context (passed down along all calls)
void print_cpu_info(cpu_t cpu_ctx){
    print_cpu_cpuid_supported_leafs(cpu_ctx, 2);
    print_cpu_processor_identifiers(cpu_ctx);
    print_cpu_specification(cpu_ctx);
    print_cpu_cache_info(cpu_get_cache_info());
    //print_cpu_clock_info();   Buggy as fuck
    print_cpu_features(cpu_ctx);
}