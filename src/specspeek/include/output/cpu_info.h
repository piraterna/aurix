#pragma once
#include <system/hardware/CPU/cpu.h>

/// @brief Prints the processors supported Standard and Extended Leaves
/// @param cpu_ctx cpu context
/// @param min_verbosity verbosity required to print
void print_cpu_cpuid_supported_leafs(cpu_t cpu_ctx, int min_verbosity);

/// @brief Prints all identifying information about the processor
/// @param cpu_ctx cpu context
void print_cpu_processor_identifiers(cpu_t cpu_ctx);

/// @brief Prints standard, extended and misc features for the CPU
/// @param cpu_ctx cpu context
void print_cpu_features(cpu_t cpu_ctx);

/// @brief calls upon all cpu print functions, does not override min_verbosity
/// @param cpu_ctx cpu context (passed down along all calls)
void print_cpu_info(cpu_t cpu_ctx);

/// @brief calls upon all cpu print functions, does not override min_verbosity
/// @param cpu_ctx cpu context (passed down along all calls)
void print_cpu_info(cpu_t cpu_ctx);
