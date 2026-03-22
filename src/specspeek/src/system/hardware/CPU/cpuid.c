void cpuid(
    unsigned int leaf,
    unsigned int subleaf,
    unsigned int* eax,
    unsigned int* ebx,
    unsigned int* ecx,
    unsigned int* edx
){
    __asm__ volatile("cpuid"
                    : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                    : "a"(leaf), "c"(subleaf));
}

/// @brief gets the max supported standard leaf this hardware implementation of CPUID supports
/// @param VOID
/// @return max supported leaf (STANDARD LEAF)
unsigned int cpu_get_max_supported_leaf(void){
    unsigned int eax, ebx, ecx, edx;
    cpuid(0x00000000, 0, &eax, &ebx, &ecx, &edx);
    return eax;
}

/// @brief gets the max supported extended leaf this hardware implementation of CPUID supports
/// @param VOID
/// @return max supported leaf (EXTENDED LEAF)
unsigned int cpu_get_max_supported_extended_leaf(void){
    unsigned int eax, ebx, ecx, edx;
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    return eax;
}

/// @brief helper function to determine if a CPU supports a standard leaf function
/// @param leaf target to examine
/// @return int true/false
unsigned int cpu_supports_standard_leaf(unsigned int leaf){
    return cpu_get_max_supported_leaf() >= leaf;
}

/// @brief helper function to determine if a CPU supports a extended leaf function
/// @param leaf target to examine
/// @return int true/false
unsigned int cpu_supports_extended_leaf(unsigned int leaf){
    return cpu_get_max_supported_extended_leaf() >= leaf;
}