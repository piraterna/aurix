#include <system/hardware/CPU/cpu.h>
#include <stdio.h>

/*
    There is little to no intel support here right now, I am still activly researching 
    UPDATE: I have contacted intel asking for some documentation, they have responded and I am awaiting their responce
*/

/// @brief sets the CPU virtualisation state
/// @return 0/1 success
unsigned int cpu_get_virtualisation_enabled(){
    unsigned int eax, ebx, ecx, edx;

    IF_VENDOR_AMD({
        cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
        if (HAS_FEATURE(ecx, 2)){
            return 1;
        }
        else return 0;
    });
    return 0;
}

/// @brief gets the Virtualisation Technology Revision Number
/// @return Virtualisation Revision Number
unsigned int cpu_get_virtualisation_revision(){
    unsigned int eax, ebx, ecx, edx;

    IF_VENDOR_AMD({
        cpuid(0x8000000A, 0, &eax, &ebx, &ecx, &edx);
        return (eax & 0xFF);
    });

    return 0;
}

