#include <stdio.h>

#include <system/hardware/CPU/cpu.h>
#include <utils/arguments.h>
#include <utils/terminal.h>

#define FEATURES_PER_LINE   4

// used to determine when we need to start a new line
static int feature_count = 0;

/// @brief prints the CPU features name and its support status given the current machine
/// @param verbosity how verbose the user must be to have the feature printed
/// @param status supported or not
/// @param feature_name feature name
void cpu_print_feature(int verbosity, int status, const char* feature_name){
    IF_VERBOSE(verbosity){
        feature_count++;

        printf("%s%-8s %s%-8s%s|%-8s",
            BWHITE, feature_name,
            status != 0 ? GREEN : RED,
            status != 0 ? "TRUE" : "FALSE",
            RESET, RESET);

        if (get_arguments().put_features_on_newline || feature_count % FEATURES_PER_LINE == 0) { printf("\n"); }
    }
}

/// reset it so we can stop generating features that chill on one line
void reset_cpu_feature_count() { feature_count = 0; }

/// @brief runs through features based on the Arch
void cpu_check_standard_features(){
    unsigned int eax, ebx, ecx, edx;

    IF_VENDOR_AMD({
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);

        /*
            ECX Register Feature Bits
        */
        cpu_print_feature(0, HAS_FEATURE(ecx, 0), "SSE3");
        cpu_print_feature(0, HAS_FEATURE(ecx, 1), "PCLMUL");
        cpu_print_feature(1, HAS_FEATURE(ecx, 3), "MONITOR");
        cpu_print_feature(0, HAS_FEATURE(ecx, 9), "SSSE3");
        cpu_print_feature(0, HAS_FEATURE(ecx, 12), "FMA");
        cpu_print_feature(1, HAS_FEATURE(ecx, 13), "CMCH16B");
        cpu_print_feature(0, HAS_FEATURE(ecx, 19), "SSE4.1");
        cpu_print_feature(0, HAS_FEATURE(ecx, 20), "SSE4.2");
        cpu_print_feature(0, HAS_FEATURE(ecx, 21), "X2APIC");
        cpu_print_feature(1, HAS_FEATURE(ecx, 22), "MOVEBE");
        cpu_print_feature(0, HAS_FEATURE(ecx, 23), "POPCNT");
        cpu_print_feature(0, HAS_FEATURE(ecx, 25), "AES");
        cpu_print_feature(2, HAS_FEATURE(ecx, 26), "XSAVE");
        cpu_print_feature(2, HAS_FEATURE(ecx, 27), "OSXSAVE");
        cpu_print_feature(0, HAS_FEATURE(ecx, 28), "AVX");
        cpu_print_feature(2, HAS_FEATURE(ecx, 29), "F16C");
        cpu_print_feature(2, HAS_FEATURE(ecx, 30), "RDRAND");

        /*
            EDX Register Feature Bits
        */
        cpu_print_feature(1, HAS_FEATURE(edx, 0), "FPU");
        cpu_print_feature(0, HAS_FEATURE(edx, 1), "VME");
        cpu_print_feature(2, HAS_FEATURE(edx, 2), "DE");
        cpu_print_feature(1, HAS_FEATURE(edx, 3), "PSE");
        cpu_print_feature(2, HAS_FEATURE(edx, 4), "TSC");
        cpu_print_feature(1, HAS_FEATURE(edx, 5), "MSR");
        cpu_print_feature(0, HAS_FEATURE(edx, 6), "PAE");
        cpu_print_feature(1, HAS_FEATURE(edx, 7), "MCE");
        cpu_print_feature(2, HAS_FEATURE(edx, 8), "CPCH8B");
        cpu_print_feature(1, HAS_FEATURE(edx, 9), "APIC");
        cpu_print_feature(2, HAS_FEATURE(edx, 11), "SYSENEX");
        cpu_print_feature(1, HAS_FEATURE(edx, 12), "MTRR");
        cpu_print_feature(0, HAS_FEATURE(edx, 13), "PGE");
        cpu_print_feature(1, HAS_FEATURE(edx, 14), "MCA");
        cpu_print_feature(2, HAS_FEATURE(edx, 15), "CMOV");
        cpu_print_feature(1, HAS_FEATURE(edx, 16), "PAT");
        cpu_print_feature(2, HAS_FEATURE(edx, 17), "PSE36");
        cpu_print_feature(1, HAS_FEATURE(edx, 19), "CFLUSH");
        cpu_print_feature(0, HAS_FEATURE(edx, 23), "MMX");
        cpu_print_feature(2, HAS_FEATURE(ecx, 24), "FXSR");
        cpu_print_feature(0, HAS_FEATURE(ecx, 25), "SSE");
        cpu_print_feature(0, HAS_FEATURE(ecx, 26), "SSE2");
        cpu_print_feature(0, HAS_FEATURE(ecx, 28), "HTT");
    });

    IF_VENDOR_INTEL({
        cpuid(0x00000001, 0, &eax, &ebx, &ecx, &edx);
        cpu_print_feature(0, HAS_FEATURE(ecx, 0), "SSE3");
        cpu_print_feature(0, HAS_FEATURE(ecx, 1), "PCLMUL");
        cpu_print_feature(0, HAS_FEATURE(ecx, 2), "DTES64");
        cpu_print_feature(1, HAS_FEATURE(ecx, 3), "MONITOR");
        cpu_print_feature(2, HAS_FEATURE(ecx, 4), "DS-CPL");
        cpu_print_feature(0, HAS_FEATURE(ecx, 5), "VMX");
        cpu_print_feature(0, HAS_FEATURE(ecx, 6), "SMX");
        cpu_print_feature(0, HAS_FEATURE(ecx, 7), "EIST");
        cpu_print_feature(0, HAS_FEATURE(ecx, 8), "TM2");
        cpu_print_feature(0, HAS_FEATURE(ecx, 9), "SSSE3");
        cpu_print_feature(1, HAS_FEATURE(ecx, 10), "CNXT-ID");
        cpu_print_feature(2, HAS_FEATURE(ecx, 11), "SDGB");
        cpu_print_feature(0, HAS_FEATURE(ecx, 12), "FMA");
        cpu_print_feature(0, HAS_FEATURE(ecx, 13), "CMCH16B");
        cpu_print_feature(1, HAS_FEATURE(ecx, 14), "XTPRUCT");
        cpu_print_feature(1, HAS_FEATURE(ecx, 15), "PDCM");
        cpu_print_feature(0, HAS_FEATURE(ecx, 17), "PCID");
        cpu_print_feature(1, HAS_FEATURE(ecx, 18), "DCA");
        cpu_print_feature(0, HAS_FEATURE(ecx, 19), "SSE4.1");
        cpu_print_feature(0, HAS_FEATURE(ecx, 20), "SSE4.2");
        cpu_print_feature(1, HAS_FEATURE(ecx, 21), "X2APIC");
        cpu_print_feature(1, HAS_FEATURE(ecx, 22), "MOVEB");
        cpu_print_feature(2, HAS_FEATURE(ecx, 23), "POPCNT");
        cpu_print_feature(2, HAS_FEATURE(ecx, 24), "TCDEADL");
        cpu_print_feature(0, HAS_FEATURE(ecx, 25), "AES-NI");
        cpu_print_feature(1, HAS_FEATURE(ecx, 26), "XSAVE");
        cpu_print_feature(1, HAS_FEATURE(ecx, 27), "OSXSAVE");
        cpu_print_feature(0, HAS_FEATURE(ecx, 28), "AVX");
        cpu_print_feature(1, HAS_FEATURE(ecx, 29), "F16C");
        cpu_print_feature(1, HAS_FEATURE(ecx, 30), "RDRAND");

        cpu_print_feature(0, HAS_FEATURE(edx, 0), "FPU");
        cpu_print_feature(0, HAS_FEATURE(edx, 1), "VME");
        cpu_print_feature(0, HAS_FEATURE(edx, 2), "DE");
        cpu_print_feature(1, HAS_FEATURE(edx, 3), "PSE");
        cpu_print_feature(2, HAS_FEATURE(edx, 4), "TSC");
        cpu_print_feature(1, HAS_FEATURE(edx, 5), "MSR");
        cpu_print_feature(0, HAS_FEATURE(edx, 6), "PAE");
        cpu_print_feature(0, HAS_FEATURE(edx, 7), "MCE");
        cpu_print_feature(2, HAS_FEATURE(edx, 8), "CX8");
        cpu_print_feature(1, HAS_FEATURE(edx, 9), "APIC");
        cpu_print_feature(2, HAS_FEATURE(edx, 11), "SEP");
        cpu_print_feature(1, HAS_FEATURE(edx, 12), "MTRR");
        cpu_print_feature(0, HAS_FEATURE(edx, 13), "PGE");
        cpu_print_feature(1, HAS_FEATURE(edx, 14), "MCA");
        cpu_print_feature(1, HAS_FEATURE(edx, 15), "CMOV");
        cpu_print_feature(1, HAS_FEATURE(edx, 16), "PAT");
        cpu_print_feature(1, HAS_FEATURE(edx, 17), "PSE36");
        cpu_print_feature(2, HAS_FEATURE(edx, 18), "PSN");
        cpu_print_feature(1, HAS_FEATURE(edx, 19), "CLFLSH");
        cpu_print_feature(1, HAS_FEATURE(edx, 21), "DS");
        cpu_print_feature(1, HAS_FEATURE(edx, 22), "ACPI");
        cpu_print_feature(2, HAS_FEATURE(edx, 23), "MMX");
        cpu_print_feature(0, HAS_FEATURE(edx, 24), "FXSR");
        cpu_print_feature(1, HAS_FEATURE(edx, 25), "SSE");
        cpu_print_feature(1, HAS_FEATURE(edx, 26), "SSE2");
        cpu_print_feature(1, HAS_FEATURE(edx, 27), "SS");
        cpu_print_feature(0, HAS_FEATURE(edx, 28), "HTT");
        cpu_print_feature(1, HAS_FEATURE(edx, 29), "TM");
        cpu_print_feature(1, HAS_FEATURE(edx, 31), "PBE");
    });
}

/// @brief gets the extended features from the CPUID (leaf 7 for AMD)
void cpu_check_extended_features(){
    unsigned int eax, ebx, ecx, edx;
    IF_VENDOR_AMD({
        /*
            EBX Register Feature Bits (0x00000007)
        */
        cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx);
        cpu_print_feature(1, HAS_FEATURE(ebx, 0), "FSGSBASE");
        cpu_print_feature(0, HAS_FEATURE(ebx, 3), "BMI1");
        cpu_print_feature(0, HAS_FEATURE(ebx, 5), "AVX2");
        cpu_print_feature(1, HAS_FEATURE(ebx, 7), "SMEP");
        cpu_print_feature(0, HAS_FEATURE(ebx, 8), "BMI2");
        cpu_print_feature(2, HAS_FEATURE(ebx, 10), "INVPCID");
        cpu_print_feature(0, HAS_FEATURE(ebx, 12), "PQM");
        cpu_print_feature(0, HAS_FEATURE(ebx, 15), "PQE");
        cpu_print_feature(0, HAS_FEATURE(ebx, 18), "RDSEED");
        cpu_print_feature(0, HAS_FEATURE(ebx, 19), "ADX");
        cpu_print_feature(1, HAS_FEATURE(ebx, 20), "SMAP");
        cpu_print_feature(2, HAS_FEATURE(ebx, 23), "CLFLSHOP");
        cpu_print_feature(0, HAS_FEATURE(ebx, 24), "CLWB");
        cpu_print_feature(0, HAS_FEATURE(ebx, 29), "SHA");

        /*
            ECX Register Feature Bits (0x00000007)
        */
        cpu_print_feature(2, HAS_FEATURE(ebx, 2), "UMIP");
        cpu_print_feature(2, HAS_FEATURE(ebx, 3), "PKU");
        cpu_print_feature(2, HAS_FEATURE(ebx, 4), "OSPKE");
        cpu_print_feature(2, HAS_FEATURE(ebx, 7), "CETSS");
        cpu_print_feature(1, HAS_FEATURE(ebx, 9), "VAES");
        cpu_print_feature(0, HAS_FEATURE(ebx, 10), "VPCMUL");
        cpu_print_feature(0, HAS_FEATURE(ebx, 16), "LA57");
        cpu_print_feature(1, HAS_FEATURE(ebx, 22), "RDPID");
        cpu_print_feature(2, HAS_FEATURE(ebx, 24), "BSLKTRP");
    });

    IF_VENDOR_INTEL({
        cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx);
        cpu_print_feature(1, HAS_FEATURE(ebx, 0), "FGSBASE");
        cpu_print_feature(1, HAS_FEATURE(ebx, 1), "TSCADJU");
        cpu_print_feature(1, HAS_FEATURE(ebx, 2), "SGX");
        cpu_print_feature(0, HAS_FEATURE(ebx, 3), "BMI1");
        cpu_print_feature(0, HAS_FEATURE(ebx, 4), "HLE");
        cpu_print_feature(0, HAS_FEATURE(ebx, 5), "AVX2");
        cpu_print_feature(0, HAS_FEATURE(ebx, 7), "SMEP");
        cpu_print_feature(0, HAS_FEATURE(ebx, 8), "BMI2");
        cpu_print_feature(1, HAS_FEATURE(ebx, 9), "MOVSB");
        cpu_print_feature(2, HAS_FEATURE(ebx, 10), "INVID");
        cpu_print_feature(1, HAS_FEATURE(ebx, 11), "RTM");
        cpu_print_feature(0, HAS_FEATURE(ebx, 12), "RDT-M");
        cpu_print_feature(0, HAS_FEATURE(ebx, 14), "MPX");
        cpu_print_feature(2, HAS_FEATURE(ebx, 15), "RDT-A");
        cpu_print_feature(0, HAS_FEATURE(ebx, 18), "RDSEED");
        cpu_print_feature(0, HAS_FEATURE(ebx, 19), "ADX");
        cpu_print_feature(0, HAS_FEATURE(ebx, 20), "SMAP");
        cpu_print_feature(2, HAS_FEATURE(ebx, 21), "AVXIFMA");
        cpu_print_feature(1, HAS_FEATURE(ebx, 23), "CLFLSOP");
        cpu_print_feature(0, HAS_FEATURE(ebx, 24), "CLWB");
        cpu_print_feature(0, HAS_FEATURE(ebx, 29), "SHA");

        /*
            Intel ECX features
        */
        cpu_print_feature(0, HAS_FEATURE(ecx, 2), "UMIP");
        cpu_print_feature(0, HAS_FEATURE(ecx, 3), "PKU");
        cpu_print_feature(0, HAS_FEATURE(ecx, 4), "OPSKE");
        cpu_print_feature(0, HAS_FEATURE(ecx, 8), "GFNI");
        cpu_print_feature(0, HAS_FEATURE(ecx, 9), "VAES");
        cpu_print_feature(1, HAS_FEATURE(ecx, 10), "VPCLMUL");
        // theres a gap here becuase theres no need for that many forms of AVX, seriously what is AVX512_VPOPCNTDQ, no need
        cpu_print_feature(0, HAS_FEATURE(ecx, 16), "LA57");
        cpu_print_feature(1, HAS_FEATURE(ecx, 22), "RDPID");
        cpu_print_feature(0, HAS_FEATURE(ecx, 23), "KL");
        cpu_print_feature(2, HAS_FEATURE(ecx, 24), "LKBSDET");
        cpu_print_feature(1, HAS_FEATURE(ecx, 27), "MOVDIRI");
        cpu_print_feature(1, HAS_FEATURE(ecx, 28), "MOVD64");
        cpu_print_feature(1, HAS_FEATURE(ecx, 29), "ENQCMD");
        cpu_print_feature(1, HAS_FEATURE(ecx, 30), "SGX_LC");
        cpu_print_feature(1, HAS_FEATURE(ecx, 31), "PKS");
    });
}

void cpu_check_misc_features(){
    unsigned int eax, ebx, ecx, edx;
    cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);

    IF_VENDOR_AMD({
        cpu_print_feature(0, HAS_FEATURE(ecx, 2), "SVM");
        cpu_print_feature(0, HAS_FEATURE(ecx, 8), "3DPRFTCH");
        cpu_print_feature(0, HAS_FEATURE(ecx, 11), "XOP");
        cpu_print_feature(0, HAS_FEATURE(ecx, 13), "WDT");
        cpu_print_feature(0, HAS_FEATURE(ecx, 16), "FMA4");
        cpu_print_feature(1, HAS_FEATURE(ecx, 17), "TCE");
        cpu_print_feature(1, HAS_FEATURE(ecx, 21), "TBM");
        cpu_print_feature(2, HAS_FEATURE(ecx, 22), "TOPOEXTN");

        cpu_print_feature(0, HAS_FEATURE(edx, 31), "3DNOW");
        cpu_print_feature(1, HAS_FEATURE(edx, 30), "3DNOWEX");
        cpu_print_feature(0, HAS_FEATURE(edx, 29), "LONGMD");
        cpu_print_feature(0, HAS_FEATURE(edx, 26), "1GBPAGE");
        cpu_print_feature(1, HAS_FEATURE(edx, 22), "MMXEXT");
        cpu_print_feature(0, HAS_FEATURE(edx, 20), "NXBIT");
    });

    cpuid(0x80000007, 0, &eax, &ebx, &ecx, &edx);

    IF_VENDOR_AMD({
        cpu_print_feature(0, HAS_FEATURE(edx, 0), "THERMON");
        cpu_print_feature(0, HAS_FEATURE(edx, 3), "THRMTRP");
        cpu_print_feature(0, HAS_FEATURE(edx, 4), "HTC");
        cpu_print_feature(0, HAS_FEATURE(edx, 9), "COREPB");
        
        cpu_print_feature(0, HAS_FEATURE(ebx, 0), "CLZERO");
        cpu_print_feature(0, HAS_FEATURE(ebx, 3), "INVLPGB");
        cpu_print_feature(0, HAS_FEATURE(ebx, 4), "RDPRU");
        cpu_print_feature(0, HAS_FEATURE(ebx, 8), "MEMBENF");
        cpu_print_feature(0, HAS_FEATURE(ebx, 14), "IBRS");
        cpu_print_feature(0, HAS_FEATURE(ebx, 15), "STIBP");
        cpu_print_feature(0, HAS_FEATURE(ebx, 24), "SSBD");
        cpu_print_feature(0, HAS_FEATURE(ebx, 27), "CPPC");
        cpu_print_feature(0, HAS_FEATURE(ebx, 28), "PSFD");
    });
}