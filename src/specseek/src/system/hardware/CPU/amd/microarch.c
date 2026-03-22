#include <system/hardware/CPU/microarch.h>

/// @brief Returns the AMD microarchitecture name based on family and model, the Linux src code was huge for this
/// @param family CPU family
/// @param model CPU model
/// @return const char* string describing the microarchitecture
const char* amd_cpu_get_microarch(unsigned int family, unsigned int model, unsigned int ext_model) {
    switch(family){
        case 0x05: return "K5";
        case 0x06: return "K7";
        case 0x0F: return "K8";
        case 0x10: return "K10";
        case 0x11: return "K10 + K8 Hybrid";
        case 0x12: return "Llano";
        case 0x14: return "Bobcat";
        case 0x15:
            switch (model){
                case 0x00:
                    switch (ext_model){
                        case 0x00:
                            return "Bulldozer";
                        case 0x01:
                        case 0x02:
                            return "Piledriver";
                        case 0x03:
                            return "Steamroller";
                        case 0x06:
                        case 0x07:
                            return "Excavator";
                        default:
                            return "Unknown Family 21 Microarch";
                    }
                case 0x01:
                case 0x02:
                case 0x03:
                    return "Piledriver";
                case 0x05:
                    return "Excavator";
                case 0x08:
                    return "Steamroller";
                default:
                    return "Unknown Family 21 Microarch";
            }
        case 0x16:
            switch (ext_model){
                case 0x00:
                    return "Jaguar";
                case 0x03:
                    return "Puma";
                default:
                    return "Unknown Family 22 Microarch";
            }
        case 0x17:
            switch (ext_model){
                case 0x00:
                case 0x01:
                    return "Summit Ridge (Zen)";
                case 0x08:
                    return "Pinnacle Ridge (Zen +)";
                case 0x02:
                case 0x03:
                case 0x07:
                    return "Matisse (Zen 2)";
                default:
                    return "Unknown AMD Zen Microarch";
            }
        case 0x18:
            return "Zen Dhyana (AMD + Chengdu Haiguang)";
        case 0x19:
            switch (model){
                case 0x00 ... 0x0F:
                case 0x20 ... 0x5F:
                    return "Vermeer (Zen 3)";
                case 0x10 ... 0x1F:
                case 0x60 ... 0xAF:
                    return "Raphael (Zen 4)";
                default:
                    return "Unknown Zen 3 / Zen 4 Processor";
            }
        case 0x1A:
            switch (model){
                case 0x00 ... 0x2F:
                case 0x40 ... 0x4F:
                case 0x60 ... 0x7F:
                    return "Granite Ridge (Zen 5)";
                case 0x50 ... 0x5F:
                case 0x90 ... 0xAF:
                case 0xC0 ... 0xCF:
                    return "Medusa Point (Zen 6)";
                default:
                    return "Unknown Zen 5 / Zen 6 Processor";
            }
        default:
            return "Unknown AMD Microarch";
    }
}
