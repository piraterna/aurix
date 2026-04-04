#ifndef _INTEL_PROCESSORS_H
#define _INTEL_PROCESSORS_H    1

extern unsigned int performance_core_count;
extern unsigned int efficient_core_count;

unsigned int intel_cpu_get_performance_core_count();
unsigned int intel_cpu_get_efficient_core_count();

/// @brief core APIC structure
typedef struct {
    unsigned int apic_id;
    unsigned int core_type;
} core_apic_t;

#endif