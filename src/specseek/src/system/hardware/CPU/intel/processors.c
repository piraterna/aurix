#define _GNU_SOURCE

#include <system/hardware/CPU/cpu.h>
#include <system/hardware/CPU/specifications.h>
#include <system/hardware/CPU/intel/processors.h>
#include <system/hardware/CPU/affinity.h>
#include <utils/terminal.h>
#include <utils/arguments.h>
#include <stdio.h>
#include <stdlib.h>

unsigned int efficient_core_count = 0;
unsigned int performance_core_count = 0;

/// @brief gets the ammount of CPU threads
/// @return unsigned int threads
unsigned int intel_cpu_get_thread_count_per_core(){
    unsigned int eax, ebx, ecx, edx;
    if (cpu_get_max_supported_leaf() >= 0x1F){
        return 0;
    }else{
        for (unsigned int i = 0; i < 5; i++){
            cpuid(0x0B, i, &eax, &ebx, &ecx, &edx);

            unsigned int topology_type = (ecx >> 8) & 0xFF;
            if(topology_type == 1) return ebx;
        }
    }
    return 251;
}

/// @brief INTEL ONLY Gets the logical processor count for the CPU Package
/// @return Logicial Processor Count
unsigned int intel_cpu_get_logical_processor_count(void) {
    unsigned int eax, ebx, ecx, edx;

    unsigned int count = 0;
    unsigned int leaf = (cpu_get_max_supported_leaf() >= 0x1F) ? 0x1F : 0x0B;

    for (unsigned int level = 0; level < 32; ++level) {
        cpuid(leaf, level, &eax, &ebx, &ecx, &edx);

        unsigned int level_type = (ecx >> 8) & 0xFF;
        if (level_type == 0 || ebx == 0)
            break;

        count = (ebx > count) ? ebx : count; // get the max number of logical processors at any level
    }
    return count;
}

/// @brief gets the type of hybrid core it is, only should be run on CPUs that support 1Ah
/// @return 1 (e-core) 2 (p-core)
unsigned int intel_cpu_get_hybrid_core_type(void){
    if (!cpu_supports_standard_leaf(0x1A)) {
        printf("%s%s:%d Hybrid function not supported on non hybrid CPU\nThis is an Error, please report at https://github.com/Mellurboo/SpecSeek", BRED, __FILE__, __LINE__);
        return 0;
    }
    unsigned int eax, ebx, ecx, edx;
    cpuid(0x1A, 0, &eax, &ebx, &ecx, &edx);

    unsigned int core_type = (eax >> 24) & 0xFF;
    return core_type;
}

/// @brief gets the APIC ID of the Intel CPU to differenciate between physical cores
/// @param VOID
/// @return apic id
unsigned int intel_cpu_get_apic_id(void){
    unsigned int eax, ebx, ecx, edx;
    cpuid(0x0000000B, 1, &eax, &ebx, &ecx, &edx);
    return edx;
}

/// @brief SMT (bit width) for CPU physical cores and threads
/// @param VOID
/// @return SMT Bit witdth
unsigned int intel_cpu_get_smt_mask_width(void) {
    unsigned int eax, ebx, ecx, edx;
    unsigned int smt_width = 0;

    // Loop levels to find SMT width
    for (unsigned int level = 0; level < 5; level++) {
        cpuid(0x0B, level, &eax, &ebx, &ecx, &edx);
        unsigned int level_type = (ecx >> 8) & 0xFF;
        if (level_type == 1) {
            smt_width = eax & 0x1F; // EAX[4:0] = bit width
            break;
        }
    }

    return smt_width;
}

/// @brief gets the physical core count of the intel CPU
/// @param VOID
/// @return unsigned int core count
unsigned int intel_cpu_get_physical_core_count(void) {
    if (cpu_get_max_supported_leaf() >= 0x1F){
        IF_VERBOSE(2){
            printf("This processor appears to be a 0x1F CPU\n");
        }

        unsigned int total_cores = 0;
        unsigned int processors = intel_cpu_get_logical_processor_count();
        core_apic_t *cores = malloc(sizeof(core_apic_t) * processors);

        for(unsigned int i = 0; i < processors; i++){
            #if _INTEL_AFFINITY == 1
            if (cpu_set_affinity(i) != 0){
                printf("%s%s:%d SpecSeek failed to set affinity on pass %d\nThis is an Error, please report at https://github.com/Mellurboo/SpecSeek", BRED, __FILE__, __LINE__, i);
                continue;   // this will mean we can see if they all fail or not? which might be helpful later.
            }
            #endif

            unsigned int apic_id = intel_cpu_get_apic_id();
            unsigned int core_type = intel_cpu_get_hybrid_core_type();
            unsigned int core_id = apic_id >> intel_cpu_get_smt_mask_width();

            int found = 0;
            for (unsigned int j = 0; j < total_cores; j++) {
                if (cores[j].apic_id == core_id) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                cores[total_cores].apic_id = core_id;
                cores[total_cores].core_type = core_type;
                total_cores++;
            }
        }

        // determine what kinda cores we have
        for (unsigned int i = 0; i < total_cores; i++){
            if (cores[i].core_type == 0x20) efficient_core_count++;
            if (cores[i].core_type == 0x40) performance_core_count++;
        }

        free(cores);
        return performance_core_count + efficient_core_count;

    }else {
        return intel_cpu_get_logical_processor_count() / intel_cpu_get_thread_count_per_core();
    }

    return 0;
}

unsigned int intel_cpu_get_crystal_clock_speed() {
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;

    if (!cpu_supports_standard_leaf(0x15)) {
        IF_VERBOSE(2) {
            printf("%sCPUID leaf 0x15 not supported on this CPU\n", BYELLOW);
        }
        return 0;
    }

    cpuid(0x15, 0, &eax, &ebx, &ecx, &edx);

    if (ecx == 0) {
        return 0;
    }

    return ecx;
}

cpu_cache_info_t intel_cpu_get_cache_info(void) {
    cpu_cache_info_t info = {0,0,0,0};
    unsigned int eax, ebx, ecx, edx;
    unsigned int level = 0;

    while (1) {
        cpuid(0x04, level, &eax, &ebx, &ecx, &edx);
        unsigned int cache_type = eax & 0x1F;
        if (cache_type == 0) break;

        unsigned int cache_level = (eax >> 5) & 0x7;
        unsigned int ways = ((ebx >> 22) & 0x3FF) + 1;
        unsigned int partitions = ((ebx >> 12) & 0x3FF) + 1;
        unsigned int line_size = (ebx & 0xFFF) + 1;
        unsigned int sets = ecx + 1;
        unsigned int cache_size_kb = (ways * partitions * line_size * sets) / 1024;

        switch (cache_level) {
            case 1:
                if (cache_type == 1) info.l1d = cache_size_kb;
                else if (cache_type == 2) info.l1i = cache_size_kb;
                break;
            case 2:
                info.l2 = cache_size_kb;
                break;
            case 3:
                info.l3 = cache_size_kb;
                break;
        }

        level++;
    }

    return info;
}

/// @brief helper function to return the ammount of performance cores on a hybrid CPU
/// @param  VOID
/// @return unsigned int performance cores
unsigned int intel_cpu_get_performance_core_count(void){
    return performance_core_count;
}

/// @brief helper function to return the ammount of efficent cores on a hybrid CPU
/// @param  VOID
/// @return unsigned in efficent cores
unsigned int intel_cpu_get_efficient_core_count(void){
    return efficient_core_count;
}
