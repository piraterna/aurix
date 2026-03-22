#include <string.h>
#include <stdio.h>

#include <system/hardware/CPU/cpu.h>
#include <system/hardware/CPU/intel/processors.h>
#include <system/hardware/CPU/specifications.h>
#include <system/hardware/CPU/virtualisation.h>
#include <system/hardware/CPU/microarch.h>
#include <utils/arguments.h>
#include <utils/terminal.h>

/// @brief Build the CPU information by populating a cpu_t struct
/// @return cpu_t CPU info structure.
cpu_t init_cpu(void) {
    cpu_t cpu;
    cpu.name        = cpu_get_name();
    cpu.model       = cpu_get_full_model();
    cpu.base_model  = cpu_get_base_model();
    cpu.ext_model   = cpu_get_extended_model();
    cpu.family      = cpu_get_family();
    cpu.base_family = cpu_get_base_family();
    cpu.ext_family  = cpu_get_extended_family();
    cpu.vendor      = cpu_get_vendor();
    cpu.revision    = cpu_get_revision();

    cpu.virtualisation = cpu_get_virtualisation_enabled();

    cpu.logical_processors  = cpu_get_logical_processor_count();
    cpu.physical_processors = cpu_get_physical_core_count();
    cpu.thread_count_per_core    = cpu_get_thread_count_per_core();

    IF_VENDOR_INTEL({
        cpu.performance_cores   = intel_cpu_get_performance_core_count();
        cpu.efficient_cores     = intel_cpu_get_efficient_core_count();
    });

    return cpu;
}

/// @brief Also known as the CPU Brand String
const char* cpu_get_name() {
    static char cpu_brand_string[49];
    unsigned int chunks[12];

    for (int i = 0; i < 3; i++) {
        unsigned int eax, ebx, ecx, edx;
        cpuid(0x80000002 + i, 0, &eax, &ebx, &ecx, &edx);
        IF_VERBOSE(3) { PRINT_REGISTER_VALUES() }
        chunks[i * 4 + 0] = eax;
        chunks[i * 4 + 1] = ebx;
        chunks[i * 4 + 2] = ecx;
        chunks[i * 4 + 3] = edx;
    }

    memcpy(cpu_brand_string, chunks, sizeof(chunks));
    cpu_brand_string[48] = '\0';
    return cpu_brand_string;
}

/// @brief Gets the CPU vendor string
const char* cpu_get_vendor() {
    static char vendor_string[13];
    unsigned int eax, ebx, ecx, edx;
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    IF_VERBOSE(3) { PRINT_REGISTER_VALUES() }

    memcpy(vendor_string + 0, &ebx, 4);
    memcpy(vendor_string + 4, &edx, 4);
    memcpy(vendor_string + 8, &ecx, 4);
    vendor_string[12] = '\0';
    return vendor_string;
}

/// @brief Returns the raw base model field (not display model)
unsigned int cpu_get_base_model() {
    unsigned int eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    IF_VERBOSE(3) { PRINT_REGISTER_VALUES() }

    return (eax >> 4) & 0xF;  // Base model
}

/// @brief Returns extended model field
unsigned int cpu_get_extended_model() {
    unsigned int eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    IF_VERBOSE(3) { PRINT_REGISTER_VALUES() }

    return (eax >> 16) & 0xF;
}

/// @brief Gets the CPU family (combines extended if needed)
unsigned int cpu_get_family() {
    unsigned int eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    IF_VERBOSE(3) { PRINT_REGISTER_VALUES() }

    unsigned int base_family = cpu_get_base_family();
    unsigned int ext_family = cpu_get_extended_family();

    if (base_family == 0xF) {
        return base_family + ext_family;
    } else {
        return base_family;
    }
}

/// @brief gets only the base family from the CPU
/// @return base family int
unsigned int cpu_get_base_family(){
    unsigned int eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    IF_VERBOSE(3) { PRINT_REGISTER_VALUES() }

    return (eax >> 8) & 0xF; 
}

/// @brief gets only the extended family from the CPU
/// @return extended family int
unsigned int cpu_get_extended_family(){
    unsigned int eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    IF_VERBOSE(3) { PRINT_REGISTER_VALUES() }

    return (eax >> 20) & 0xFF; 
}

/// @brief Returns the final Model based on CPUID
unsigned int cpu_get_full_model() {
    unsigned int eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    IF_VERBOSE(3) { PRINT_REGISTER_VALUES() }

    unsigned int base_model = (eax >> 4) & 0xF;
    unsigned int ext_model = (eax >> 16) & 0xF;
    unsigned int base_family = (eax >> 8) & 0xF;

    if (base_family == 0x6 || base_family == 0xF) {
        return (ext_model << 4) | base_model;
    } else {
        return base_model;
    }
}

/// @brief get the CPU stepping value
/// @return int stepping/revision
unsigned int cpu_get_revision() {
    unsigned int eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    IF_VERBOSE(3) { PRINT_REGISTER_VALUES() }
    return eax & 0xF;
}

/// @brief gets the amount of physical CPU cores in the package
/// @return int physical cores
unsigned int cpu_get_physical_core_count(){
    IF_VENDOR_AMD({return amd_cpu_get_physical_core_count();});
    IF_VENDOR_INTEL({return intel_cpu_get_physical_core_count();});
    return -1;
}

/// @brief gets how many thread each CPU core has
/// @return uint physical cores
unsigned int cpu_get_thread_count_per_core(){
    IF_VENDOR_AMD({return amd_cpu_get_thread_count_per_core();});
    IF_VENDOR_INTEL({return intel_cpu_get_thread_count_per_core();});
    return -1;
}

/// @brief gets the amount of threads from the cpu
/// @return int threads
unsigned int cpu_get_logical_processor_count(){
    IF_VENDOR_AMD({return amd_cpu_get_logical_processor_count();});
    IF_VENDOR_INTEL({return intel_cpu_get_logical_processor_count();});
    return -1;
}

/// @brief Get CPU frequency in Hz
/// @return nominal CPU frequency (core clock) or 0 if unavailable
unsigned int cpu_get_nominal_frequency(void) {
    IF_VENDOR_INTEL({return intel_cpu_get_crystal_clock_speed();});
    IF_VENDOR_AMD({return amd_cpu_get_nominal_core_clock();});
    return 0;
}

cpu_cache_info_t cpu_get_cache_info(void) {
    IF_VENDOR_INTEL({return intel_cpu_get_cache_info();});
    IF_VENDOR_AMD({return amd_cpu_get_cache_info();});

    cpu_cache_info_t empty = {0,0,0,0};
    return empty;
}


/// @brief platform agnostic function call to get the microarch for the CPU Vendor
/// @param family CPUID Family
/// @param base_model CPUID base_model
/// @param ext_model CPUID ext_model
/// @param stepping CPUID stepping (utilised in INTEL only)
/// @return const char microarch name
const char* cpu_get_microarch(unsigned int family,
    unsigned int base_model,
    unsigned int ext_model,
    unsigned int stepping){
    IF_VENDOR_AMD({return amd_cpu_get_microarch(family, base_model, ext_model);});
    IF_VENDOR_INTEL({return intel_cpu_get_microarch(family, base_model, ext_model, stepping);});
    return NULL;
}
 