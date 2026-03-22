#ifndef _CPU_H
#define _CPU_H  1

#include <string.h>

#define _AMD_VENDOR_STRING     "AuthenticAMD"
#define _INTEL_VENDOR_STRING   "GenuineIntel"

#define VENDOR_COLOUR (strcmp(cpu_get_vendor(), _AMD_VENDOR_STRING) == 0 ? RED : CYAN)
#define BOLD_VENDOR_COLOUR (strcmp(cpu_get_vendor(), _AMD_VENDOR_STRING) == 0 ? BRED : BCYAN)

#define HAS_FEATURE(reg, bit) (((reg) & (1 << (bit))) != 0)
#define SUPPORTS_LEAF(leaf) if ()

#define IF_VENDOR_AMD(amd_arch_code) \
    do { if (!memcmp(cpu_get_vendor(), _AMD_VENDOR_STRING, (unsigned long)12)) { amd_arch_code } } while (0)
#define IF_VENDOR_INTEL(intel_arch_code) \
    do { if (!memcmp(cpu_get_vendor(), _INTEL_VENDOR_STRING, (unsigned long)12)) { intel_arch_code } } while (0)

#define PRINT_REGISTER_VALUES() printf("%s%s:%s():%d eax:0x%08X, ebx:0x%08X, ecx:0x%08X, edx:0x%08X\n", BMAGENTA, __FILE__, __PRETTY_FUNCTION__, __LINE__, (eax), (ebx), (ecx), (edx));

/// @brief CPU Structure
typedef struct cpu
{
    const char* name;
    const char* vendor;
    unsigned int model;
    unsigned int base_model;
    unsigned int ext_model;
    unsigned int family;
    unsigned int base_family;
    unsigned int ext_family;
    unsigned int revision;

    unsigned int logical_processors;
    unsigned int physical_processors;
    unsigned int thread_count_per_core;
    unsigned int performance_cores;
    unsigned int efficient_cores;

    unsigned int virtualisation;
    unsigned int virtualisation_revision;

    unsigned int max_standard_leaf;
    unsigned int max_extended_leaf;
}cpu_t;

typedef struct {
    unsigned int l1d;
    unsigned int l1i;
    unsigned int l2;
    unsigned int l3;
} cpu_cache_info_t;

/// @brief CPUID instruction wrapper that places results from the leaf and subleaf into respective locations
/// @param leaf CPUID Leaf (code)
/// @param subleaf CPUID Subleaf (int argument)
/// @param eax u32 contains part of CPUID results
/// @param ebx u32 contains part of CPUID results
/// @param ecx u32 contains part of CPUID results
/// @param edx u32 contains part of CPUID results
void cpuid(
    unsigned int leaf,
    unsigned int subleaf,
    unsigned int* eax,
    unsigned int* ebx,
    unsigned int* ecx,
    unsigned int* edx
);

unsigned int cpu_get_max_supported_leaf();
unsigned int cpu_get_max_supported_extended_leaf();

/// @brief Constructs a CPU variable
/// @return cpu_t
cpu_t init_cpu();

/// @brief Uses the CPUID function to return a text buffer containing the CPU Vendor
/// @return character array
const char* cpu_get_vendor();

/// @brief Also known as the CPU Brand String
/// @return CPU Name e.g. Ryzen 5 9600x 6 Core Processor
const char* cpu_get_name();

/// @brief prints the CPU features name and its support status given the current machine
/// @param verbosity how verbose the user must be to have the feature printed
/// @param status supported or not
/// @param feature_name feature name
void cpu_print_feature(int verbosity, int status, const char* feature_name);

/// reset it so we can stop generating features that chill on one line
void reset_cpu_feature_count();

/// @brief runs through features based on the Arch
void cpu_check_standard_features();

/// @brief gets the extended features from the CPUID (leaf 7 for AMD)
void cpu_check_extended_features();

/// @brief checks misc features that are important
void cpu_check_misc_features();

/// @brief helper function to determine if a CPU supports a standard leaf function
/// @param leaf target to examine
/// @return int true/false
unsigned int cpu_supports_standard_leaf(unsigned int leaf);

/// @brief helper function to determine if a CPU supports a extended leaf function
/// @param leaf target to examine
/// @return int true/false
unsigned int cpu_supports_extended_leaf(unsigned int leaf);

/// @brief get the CPU model ID leaf 1, 0
/// @return unsigned int
unsigned int cpu_get_base_model();

/// @brief Gets the extended model number of the CPU, this isnt really that useful
/// @return unsigned int full model
unsigned int cpu_get_extended_model();

/// @brief Returns the final "Display Model" based on CPUID
unsigned int cpu_get_full_model();

/// @brief Gets the CPU family ID combining extended and base family
/// @return unsigned int full family number
unsigned int cpu_get_family();

/// @brief gets only the base family from the CPU
/// @return base family int
unsigned int cpu_get_base_family();

/// @brief gets only the extended family from the CPU
/// @return extended family int
unsigned int cpu_get_extended_family();

/// @brief Gets the current CPU revision (stepping)
/// @return unsigned int stepping number
unsigned int cpu_get_revision();

/// @brief get the amount of physical cores on the CPU
/// @return int processors
unsigned int cpu_get_physical_core_count();

/// @brief gets the amount of threads from the cpu
/// @return int threads
unsigned int cpu_get_logical_processor_count();

/// @brief gets how many thread each CPU core has
/// @return uint physical cores
unsigned int cpu_get_thread_count_per_core();

unsigned int cpu_get_nominal_frequency(void);

cpu_cache_info_t cpu_get_cache_info(void);

/// @brief platform agnostic function call to get the microarch for the CPU Vendor
/// @param family CPUID Family
/// @param base_model CPUID base_model
/// @param ext_model CPUID ext_model
/// @param stepping CPUID stepping (utilised in INTEL only)
/// @return const char microarch name
const char* cpu_get_microarch(unsigned int family,
    unsigned int base_model,
    unsigned int ext_model,
    unsigned int stepping);

#endif
