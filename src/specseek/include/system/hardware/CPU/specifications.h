#ifndef _SPECIFICATIONS_H
#define _SPECIFICATIONS_H   1

unsigned int amd_cpu_get_physical_core_count();
unsigned int amd_cpu_get_thread_count_per_core();
unsigned int amd_cpu_get_logical_processor_count();
unsigned int amd_cpu_get_nominal_core_clock();
cpu_cache_info_t amd_cpu_get_cache_info();

unsigned int intel_cpu_get_logical_processor_count();
unsigned int intel_cpu_get_thread_count_per_core();
unsigned int intel_cpu_get_physical_core_count();
unsigned int intel_cpu_get_crystal_clock_speed();
cpu_cache_info_t intel_cpu_get_cache_info();

int get_physical_cores();

#endif
