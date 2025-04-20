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

#include <lib/string.h>
#include <mm/mman.h>
#include <efi.h>
#include <efilib.h>
#include <print.h>

#include <hda.h>
#include <pciconf.h>

#include <stdint.h>
#include <stddef.h>

EFI_PCI_IO_PROTOCOL *HdaPciIoProtocol = NULL;
EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *RootBridgePciIoProtocol = NULL;

EFI_STATUS TearDownCommonBuffer(uint8_t *HostAddress,
							uint64_t Length,
							void *Mapping)
{
	EFI_STATUS Status;

	Status = HdaPciIoProtocol->Unmap(HdaPciIoProtocol, Mapping);
	if (EFI_ERROR(Status)) {
		return Status;
	}

	Status = HdaPciIoProtocol->FreeBuffer(HdaPciIoProtocol, EFI_SIZE_TO_PAGES(Length), (void *)HostAddress);
	return Status;
}

EFI_STATUS ReleaseHdaPciIoHandler()
{
	EFI_STATUS Status = EFI_SUCCESS;

	// TODO
	/*
	Status = TearDownCommonBuffer((uint8_t *)CorbAddress, 1024, CorbMapping);
	if (!EFI_ERROR(Status)) {
		Status = TearDownCommonBuffer((uint8_t *)RirbAddress, 2048, RirbMapping);
	}
	*/

	return Status;
}

EFI_STATUS SetupCommonBuffer(uint8_t **HostAddress,
							uint64_t Length,
							void **Mapping,
							uint64_t Alignment)
{
	(void)Mapping;
	(void)Alignment;

	*HostAddress = mem_alloc(Length);
	return EFI_SUCCESS;
}

EFI_STATUS GetHdaPciIoHandler()
{
	EFI_HANDLE *DetectedHandles = NULL;
	EFI_UINTN DetectedHandlesCount = 0;
	EFI_UINTN CurrentHandle = 0;
	EFI_STATUS Status = EFI_SUCCESS;
	EFI_GUID PciIoProtocolGuid = EFI_PCI_IO_PROTOCOL_GUID;

	EFI_UINTN SegmentNumber;
	EFI_UINTN BusNumber;
	EFI_UINTN DeviceNumber;
	EFI_UINTN FunctionNumber;

	if (HdaPciIoProtocol == NULL) {
		Status = gBootServices->LocateHandleBuffer(ByProtocol,
									   &PciIoProtocolGuid,
									   NULL,
									   &DetectedHandlesCount,
									   &DetectedHandles);
  
		if (!EFI_ERROR(Status)) {
			for (CurrentHandle = 0; CurrentHandle < DetectedHandlesCount; CurrentHandle++) {
				Status = gBootServices->OpenProtocol(DetectedHandles[CurrentHandle],
												&PciIoProtocolGuid,
												(VOID**) &HdaPciIoProtocol,
												gImageHandle,
												NULL,
												EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

				Status = HdaPciIoProtocol->GetLocation(HdaPciIoProtocol,
											&SegmentNumber,
											&BusNumber,
											&DeviceNumber,
											&FunctionNumber);

				debug("GetHdaPciIoHandler(): Found device %llu:%llu:%llu\n", SegmentNumber, BusNumber, DeviceNumber, FunctionNumber);

				// This means a HDA controller was detected
				if (BusNumber == HDA_BUS && DeviceNumber == HDA_DEV && FunctionNumber == HDA_FUNC) {
					debug("Handle detected\r\n");
					return EFI_SUCCESS;
				}

				gBootServices->CloseProtocol(DetectedHandles[CurrentHandle],
									&PciIoProtocolGuid,
									gImageHandle,
									NULL);
			}
		}
	}

	return EFI_NOT_FOUND;
}

EFI_STATUS GetRootBridgeIoHandler()
{
	EFI_UINTN handleCount = 0;
	EFI_HANDLE *DetectedHandles = NULL;
	EFI_STATUS Status = EFI_SUCCESS;
	EFI_GUID PciRootBridgeIoProtocolGuid = EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID;

	if (RootBridgePciIoProtocol == NULL) {
		Status = gBootServices->LocateHandleBuffer(ByProtocol,
										&PciRootBridgeIoProtocolGuid,
										 NULL,
										 &handleCount,
										 &DetectedHandles);

		if (!EFI_ERROR(Status)) {
		  Status = gBootServices->OpenProtocol(DetectedHandles[0],
									&PciRootBridgeIoProtocolGuid,
									(void **)&RootBridgePciIoProtocol,
									gImageHandle,
									NULL,
									EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
		}
	}

	return Status;
}

EFI_STATUS GetControllerRegisterSet(PCI_HDA_REGION *PcieDeviceConfigSpace,
		HDA_CONTROLLER_REGISTER_SET *ControllerRegisterSet)
{
	uint64_t IssCount = 0;
	uint64_t OssCount = 0;
	uint64_t BssCount = 0;

	uint64_t HdaControllerBar = (PcieDeviceConfigSpace->HDBARL & 0xFFFFFFF0);

	RootBridgePciIoProtocol->Mem.Read(RootBridgePciIoProtocol, EfiPciWidthUint8, HdaControllerBar, sizeof(HDA_CONTROLLER_REGISTER_SET) / sizeof(uint8_t), (void *)ControllerRegisterSet);
	IssCount = HDA_ISS_COUNT(ControllerRegisterSet->GCAP);
	OssCount = HDA_OSS_COUNT(ControllerRegisterSet->GCAP);
	BssCount = HDA_BSS_COUNT(ControllerRegisterSet->GCAP);

	if (IssCount > 0) {
		ControllerRegisterSet->ISS = mem_alloc(IssCount * sizeof(HDA_CONTROLLER_STREAM_DESCRIPTOR));
		memset(ControllerRegisterSet->ISS, 0, IssCount * sizeof(HDA_CONTROLLER_STREAM_DESCRIPTOR));
		RootBridgePciIoProtocol->Mem.Read(RootBridgePciIoProtocol, EfiPciWidthUint8, HdaControllerBar + CALCULATE_ISSN_OFFSET(0), (sizeof(HDA_CONTROLLER_STREAM_DESCRIPTOR) * IssCount) / sizeof(uint8_t), (void *)ControllerRegisterSet->ISS);
	}

	if (OssCount > 0) {
		ControllerRegisterSet->OSS = mem_alloc(OssCount * sizeof(HDA_CONTROLLER_STREAM_DESCRIPTOR));
		memset(ControllerRegisterSet->OSS, 0, OssCount * sizeof(HDA_CONTROLLER_STREAM_DESCRIPTOR));
		RootBridgePciIoProtocol->Mem.Read(RootBridgePciIoProtocol, EfiPciWidthUint8, HdaControllerBar + CALCULATE_OSSN_OFFSET(0, ControllerRegisterSet->GCAP), (sizeof(HDA_CONTROLLER_STREAM_DESCRIPTOR) * OssCount) / sizeof(uint8_t), (void *)ControllerRegisterSet->OSS);
	}

	if (BssCount > 0) {
		ControllerRegisterSet->BSS = mem_alloc(BssCount * sizeof(HDA_CONTROLLER_STREAM_DESCRIPTOR));
		RootBridgePciIoProtocol->Mem.Read(RootBridgePciIoProtocol, EfiPciWidthUint8, HdaControllerBar + CALCULATE_BSSN_OFFSET(0, ControllerRegisterSet->GCAP), (sizeof(HDA_CONTROLLER_STREAM_DESCRIPTOR) * BssCount) / sizeof(uint8_t), (void *)ControllerRegisterSet->BSS);
	}

	return EFI_SUCCESS;
}

EFI_STATUS WritePciMemory(uint64_t Address,
						void *Value,
						uint64_t Count,
						EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width)
{
	return RootBridgePciIoProtocol->Mem.Write(RootBridgePciIoProtocol, Width, Address, Count, Value);
}

EFI_STATUS ReadPciMemory(uint64_t Address,
						void *Value,
						uint64_t Count,
						EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width)
{
	return RootBridgePciIoProtocol->Mem.Read(RootBridgePciIoProtocol, Width, Address, Count, Value);
}

EFI_STATUS WriteControllerRegister(PCI_HDA_REGION *PcieDeviceConfigSpace,
								uint64_t Offset,
								void *Value,
								uint64_t Count,
								uint8_t BarIndex,
								EFI_PCI_IO_PROTOCOL_WIDTH Width)
{
	(void)PcieDeviceConfigSpace;

	EFI_STATUS Status = EFI_SUCCESS;
	EFI_UINTN VariableWidth = 0;

	switch (Width) {
		case EfiPciWidthUint32:
			VariableWidth = sizeof(uint32_t);
			break;
		case EfiPciWidthUint16:
			VariableWidth = sizeof(uint16_t);
			break;
		case EfiPciWidthUint8:
			VariableWidth = sizeof(uint8_t);
			break;
		default:
			return EFI_INVALID_PARAMETER;
	}

	void *ReadValue = mem_alloc(VariableWidth);
	memset(ReadValue, 0, VariableWidth);

	HdaPciIoProtocol->Mem.Write(HdaPciIoProtocol, Width, BarIndex, Offset, Count, Value);
	Status = HdaPciIoProtocol->Mem.Read(HdaPciIoProtocol, Width, BarIndex, Offset, Count, ReadValue);
	
	mem_free(ReadValue);
	
	return Status;
}

EFI_STATUS ReadControllerRegister(PCI_HDA_REGION *PcieDeviceConfigSpace,
								EFI_UINT64 Offset,
								void *Value,
								EFI_UINTN Count,
								EFI_PCI_IO_PROTOCOL_WIDTH Width)
{
	EFI_UINT64 HdaControllerBar = (PcieDeviceConfigSpace->HDBARL & 0xFFFFFFF0);
	return RootBridgePciIoProtocol->Mem.Read(RootBridgePciIoProtocol, (EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH)Width, HdaControllerBar + Offset, Count, Value);
}

EFI_STATUS ReadControllerPcieConfiguration(PCI_HDA_REGION *PcieDeviceConfigSpace,
										uint64_t Offset,
										void *Value,
										uint64_t Count,
										EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width)
{
	EFI_UINT64 Address = (HDA_BUS << 24) | (HDA_DEV << 16) | (HDA_FUNC << 8) | Offset;
	return RootBridgePciIoProtocol->Pci.Read(RootBridgePciIoProtocol, Width, Address, Count, Value);
}

EFI_STATUS WriteControllerPcieConfiguration(PCI_HDA_REGION *PcieDeviceConfigSpace,
										uint64_t Offset,
										void *Value,
										uint64_t Count,
										EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width)
{
	EFI_UINT64 Address = (HDA_BUS << 24) | (HDA_DEV << 16) | (HDA_FUNC << 8) | Offset;
	return RootBridgePciIoProtocol->Pci.Write(RootBridgePciIoProtocol, Width, Address, Count, Value);
}

EFI_STATUS GetPcieConfigSpace(EFI_UINTN Bus,
							EFI_UINTN Device,
							EFI_UINTN Function,
							PCI_HDA_REGION *PcieDeviceConfigSpace)
{
	EFI_STATUS Status;
	if (PcieDeviceConfigSpace == NULL) {
		debug("PcieDeviceConfigSpace==NULL\n");
	}
	debug("Width=%u\n", (sizeof(PCI_HDA_REGION) / sizeof(uint8_t)));
	Status = RootBridgePciIoProtocol->Pci.Read(RootBridgePciIoProtocol, EfiPciIoWidthUint8,
										EFI_PCI_ADDRESS(Bus, Device, Function, 0),
										sizeof(PCI_HDA_REGION) / sizeof(EFI_UINT8),
										(void *)PcieDeviceConfigSpace);
	if (EFI_ERROR(Status)) {
		debug("GetPcieConfigSpace(): %s (%x)\n", efi_status_to_str(Status), Status);
	}
	return Status;
}
