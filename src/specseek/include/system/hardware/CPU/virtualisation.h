#include <system/hardware/CPU/cpu.h>

/// @brief sets the CPU virtualisation state
/// @return 0/1 success
unsigned int cpu_get_virtualisation_enabled();

/// @brief gets the Virtualisation Technology Revision Number
/// @return Virtualisation Revision Number
unsigned int cpu_get_virtualisation_revision();