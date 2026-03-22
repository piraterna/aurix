#include <system/hardware/CPU/microarch.h>

const char* intel_cpu_get_microarch(unsigned int family,
    unsigned int base_model,
    unsigned int ext_model,
    unsigned int stepping) {

    unsigned int displayed_model = base_model;
    if (family == 6 || family == 15) {
        displayed_model = (ext_model << 4) | base_model;
    }

    switch (family) {
        case 4:
            return "Intel 486";
        case 5:
            switch (displayed_model) {
                case 0x01 ... 0x08:
                    return "P5";
                case 0x09 ... 0x0A:
                    return "Lakemont";
                default:
                    return "Unknown Intel Processor";
            }
        case 6:
            switch (displayed_model) {
                case 0x01 ... 0x0F: return "Pentium Pro / Pentium II / Pentium III";
                case 0x16: return "Pentium M (Dothan)";
                case 0x1A: return "Nehalem";
                case 0x1E: return "Nehalem";
                case 0x1F: return "Nehalem";
                case 0x2A: return "Sandy Bridge";
                case 0x2D: return "Sandy Bridge-E";
                case 0x3A: return "Ivy Bridge";
                case 0x3E: return "Ivy Bridge-E";
                case 0x3C: return "Haswell";
                case 0x3F: return "Haswell";
                case 0x45: return "Haswell-E";
                case 0x46: return "Haswell-E";
                case 0x3D: return "Broadwell";
                case 0x47: return "Broadwell-E";
                case 0x4E: return "Broadwell-EP";
                case 0x56: return "Broadwell-DE";
                case 0x5E: return "Skylake";
                case 0x9E: return "Coffee Lake";
                case 0x8E: return "Kaby Lake";
                case 0xA5: return "Ice Lake";
                case 0xA6: return "Tiger Lake";
                case 0x7D: return "Rocket Lake";
                case 0x7E: return "Rocket Lake";
                case 0x7F: return "Rocket Lake";
                case 0x8C: return "Tiger Lake";
                case 0x8D: return "Tiger Lake";
                case 0xA0: return "Alder Lake";
                case 0xA7: return "Alder Lake";
                case 0xB7: return "Alder Lake";
                case 0xAA: return "Meteor Lake";
                case 0xAF: return "Meteor Lake";
                case 0xBE: return "Raptor Lake";
                case 0xBF: return "Raptor Lake";
                case 0xC0: return "Arrow Lake";
                case 0xC6: return "Arrow Lake";
                case 0xC5: return "Arrow Lake";
                default:
                    return "Unknown Intel Processor";
            }
        case 15:
            if (displayed_model <= 0x0F) {
                return "NetBurst";
            }
            return "Unknown Intel Processor";
        default:
            return "Unknown Intel Microarch";
    }
}
