/*********************************************************************************/
/* Module Name:  pciconf.h                                                       */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2018-2025, Rafael Rodrigues Machado, Jozef Nagy                 */
/* All rights reserved.                                                          */
/* This program and the accompanying materials are licensed and made available   */
/* under the terms and conditions of the BSD License which accompanies           */
/* this distribution. The full text of the license may be found at               */
/* http://opensource.org/licenses/bsd-license.                                   */
/*                                                                               */
/* THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,         */
/* WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED. */
/*********************************************************************************/

#ifndef _HDA_PCICONF_H
#define _HDA_PCICONF_H

#include <hda.h>
#include <efi.h>
#include <efilib.h>

EFI_STATUS TearDownCommonBuffer(uint8_t *HostAddress,
							uint64_t Length,
							void *Mapping);

EFI_STATUS ReleaseHdaPciIoHandler();

EFI_STATUS SetupCommonBuffer(uint8_t **HostAddress,
							uint64_t Length,
							void **Mapping,
							uint64_t Alignment);

EFI_STATUS GetHdaPciIoHandler();
EFI_STATUS GetRootBridgeIoHandler();
EFI_STATUS GetControllerRegisterSet(PCI_HDA_REGION *PcieDeviceConfigSpace,
								HDA_CONTROLLER_REGISTER_SET *ControllerRegisterSet);

EFI_STATUS WritePciMemory(uint64_t Address,
						void *Value,
						uint64_t Count,
						EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width);

EFI_STATUS ReadPciMemory(uint64_t Address,
						void *Value,
						uint64_t Count,
						EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width);

EFI_STATUS WriteControllerRegister(PCI_HDA_REGION *PcieDeviceConfigSpace,
						uint64_t Offset,
						void *Value,
						uint64_t Count,
						uint8_t BarIndex,
						EFI_PCI_IO_PROTOCOL_WIDTH Width);

EFI_STATUS ReadControllerRegister(PCI_HDA_REGION *PcieDeviceConfigSpace,
							EFI_UINT64 Offset,
							void *Value,
							EFI_UINTN Count,
							EFI_PCI_IO_PROTOCOL_WIDTH Width);

EFI_STATUS ReadControllerPcieConfiguration(PCI_HDA_REGION *PcieDeviceConfigSpace,
						uint64_t Offset,
						void *Value,
						uint64_t Count,
						EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width);

EFI_STATUS WriteControllerPcieConfiguration(PCI_HDA_REGION *PcieDeviceConfigSpace,
										uint64_t Offset,
										void *Value,
										uint64_t Count,
										EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width);

EFI_STATUS GetPcieConfigSpace(EFI_UINTN Bus,
							EFI_UINTN Device,
							EFI_UINTN Function,
							PCI_HDA_REGION *PcieDeviceConfigSpace);

#endif /* _HDA_PCICONF_H */
