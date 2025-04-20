/*********************************************************************************/
/* Module Name:  hda.c                                                           */
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
#include <stdint.h>
#include <stddef.h>
#include <print.h>

#include <hda.h>
#include <pciconf.h>

// Number of rows that are updated by the timer event for debug purposes
#define REFRESH_ROWS 9
#define Bar0 0

static uint64_t CorbCommandCount = 0;

PCI_HDA_REGION *PcieDeviceConfigSpace;
uint64_t *DmaPositionBuffer = NULL;
uint64_t *AddressBSL = NULL;
int16_t *AudioBuffer = NULL;

uint64_t CorbAddress;
void *CorbMapping = NULL;
uint8_t CorbLastAddedCommandIndex = 0;

uint64_t RirbAddress;
void *RirbMapping = NULL;
uint8_t RirbLastProcessedResponseIndex = 0;

EFI_STATUS AddCommandToCorb(uint32_t VerbToSend)
{
	CorbLastAddedCommandIndex += 1;
	if (CorbLastAddedCommandIndex >= 256) {
		CorbLastAddedCommandIndex = 0;
	}

	WritePciMemory(CorbAddress + (CorbLastAddedCommandIndex * sizeof(uint32_t)), (void *)&VerbToSend, 1, EfiPciWidthUint32);
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_CORBWP, (void *)&CorbLastAddedCommandIndex, 1, Bar0, EfiPciWidthUint32);

	CorbCommandCount += 1;
	return EFI_SUCCESS;
}

EFI_STATUS ReadResponseFromRirb(uint64_t *VerbResponse)
{
	RirbLastProcessedResponseIndex += 1;
	if (RirbLastProcessedResponseIndex >= 256) {
		RirbLastProcessedResponseIndex = 0;
	}

	return ReadPciMemory(RirbAddress + (RirbLastProcessedResponseIndex * sizeof(uint64_t)), VerbResponse, 1, EfiPciWidthUint64);
}

EFI_STATUS InitHda()
{
	EFI_STATUS Status = EFI_SUCCESS;
	Status = GetHdaPciIoHandler();
	if (EFI_ERROR(Status))
		return Status;
	
	Status = GetRootBridgeIoHandler();
	if (EFI_ERROR(Status))
		return Status;

	return EFI_SUCCESS;
}

EFI_STATUS DisposeHdaLib()
{
	// TODO: Release CORB and RIRB
	//		 Close protocols
	//		 Free Variables
	//		 Close events
	return EFI_SUCCESS;
}

EFI_STATUS AllocateCORBBuffer(PCI_HDA_REGION* PcieDeviceConfigSpace)
{
	uint8_t *CorbAddressPointer = NULL;
	uint32_t ReadValue32 = 0;
	uint32_t WriteValue = 0;
	uint8_t WriteValue8 = 0;

	SetupCommonBuffer(&CorbAddressPointer, 1024, &CorbMapping, 16);
	CorbAddress = (uint64_t)CorbAddressPointer;

	WriteValue = (CorbAddress & 0xFFFFFFFF);
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_CORBLBASE,
							(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
	
	WriteValue = (CorbAddress >> 32);
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_CORBUBASE,
							(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
	
	WriteValue = 0;
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_CORBWP,
							(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);

	WriteValue = 0x8000;
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_CORBRP,
							(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);

	ReadControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_CORBRP,
							(void *)&ReadValue32, 1, EfiPciWidthUint32);

	WriteValue = 0;
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_CORBRP,
							(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);

	WriteValue8 = 0x3;
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_CORBCTL,
							(void *)&WriteValue8, 1, Bar0, EfiPciWidthUint8);

	//debug("AllocateCORBBuffer(): CorbAddress=0x%llx\n", CorbAddress);
	//debug("AllocateCORBBuffer(): CorbMapping=0x%llx\n", CorbMapping);

	return EFI_SUCCESS;
}

EFI_STATUS AllocateRIRBBuffer(PCI_HDA_REGION* PcieDeviceConfigSpace)
{
	uint8_t *RirbAddressPointer = NULL;

	uint32_t WriteValue = 0;
	uint8_t WriteValue8 = 0;

	SetupCommonBuffer(&RirbAddressPointer, 2048, &RirbMapping, 16);
	RirbAddress = (uint64_t)RirbAddressPointer;

	WriteValue = (RirbAddress & 0xFFFFFFFF);
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_RIRBLBASE,
							(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
	
	WriteValue = (RirbAddress >> 32);
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_RIRBUBASE,
							(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
	
	WriteValue = 0;
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_RIRBWP,
							(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);

	WriteValue8 = 0x2;
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_RIRBCTL,
							(void *)&WriteValue8, 1, Bar0, EfiPciWidthUint8);

	//debug("AllocateRIRBBuffer(): RirbAddress=0x%llx\n", RirbAddress);
	//debug("AllocateRIRBBuffer(): RirbMapping=0x%llx\n", RirbMapping);

	return EFI_SUCCESS;
}

EFI_STATUS FillCodecNode(PCI_HDA_REGION *PcieDeviceConfigSpace,
						  uint32_t CurrentNodeId,
						  HDA_NODE_TYPE NodeType,
						  struct Node *CurrentNode)
{
	uint32_t CurrentSubordinateNodeCount = 0;
	struct Node TempNode;

	TempNode.NodeId = CurrentNodeId;
	TempNode.NodeType = NodeType;

	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_VENDOR_ID, &TempNode.VendorId);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_REVISION_ID, &TempNode.RevisionId);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_SUBORDINATE_NODE_COUNT, &CurrentSubordinateNodeCount);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_FUNCTION_GROUP_TYPE, &TempNode.FunctionGroupType);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_AUDIO_FUNC_CAP, &TempNode.FuncCap);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_AUDIO_WIDGET_CAP, &TempNode.WidgetCap);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_SAMPLE_SIZE_RATE_CAP, &TempNode.SampleSizeRateCap);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_STREAM_FORMATS, &TempNode.StreamFormat);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_PIN_CAP, &TempNode.PinCap);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_INPUT_AMP_CAP, &TempNode.InputAmpCap);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_OUTPUT_AMP_CAP, &TempNode.OutputAmpCap);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_CONNECTION_LIST_LENGTH, &TempNode.ConnectionListLength);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_SUPPORTED_POWER_STATES, &TempNode.SupportedPowerStates);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_PROCESSING_CAP, &TempNode.ProcessingCap);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_GPIO_COUNT, &TempNode.GPIOCount);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_PARAMETER, HDA_PARAM_VOLUME_KNOB_CAP, &TempNode.VolKnobCap);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_POWER_STATE, 0, &TempNode.PowerState);
	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentNodeId, HDA_VRB_GET_CHANNEL_STREAM_ID, 0, &TempNode.ChannelStreamId);

	TempNode.StartingChildNodeAddess = HDA_SUB_NODE_COUNT_START_NODE(CurrentSubordinateNodeCount);
	TempNode.SubordinateNodeCount = HDA_SUB_NODE_COUNT_TOTAL_NODE(CurrentSubordinateNodeCount);
	TempNode.WidgetType = HDA_WIDGET_TYPE(TempNode.WidgetCap);
	TempNode.RightGain = GetAmplifierGain(PcieDeviceConfigSpace, TempNode.NodeId, true, false);
	TempNode.LeftGain = GetAmplifierGain(PcieDeviceConfigSpace, TempNode.NodeId, true, true);

	memcpy(CurrentNode, &TempNode, sizeof(struct Node));
	return EFI_SUCCESS;
}

EFI_STATUS GetNodeById(struct Node *RootNode,
					   uint32_t NodeIdToSearch,
					   struct Node *NodeDetected)
{
	struct Node *CurrentFunctionGroupNode = NULL;
	struct Node *CurrentWidgetNode = NULL;

	uint32_t CurrentFunctionGroupId = 0;
	uint32_t CurrentFgArrayIndexer = 0;

	uint32_t CurrentWidgetId = 0;
	uint32_t CurrentWidgetArrayIndexer = 0;

	for (CurrentFunctionGroupId = RootNode->StartingChildNodeAddess; CurrentFunctionGroupId < RootNode->StartingChildNodeAddess + RootNode->SubordinateNodeCount; CurrentFunctionGroupId++, CurrentFgArrayIndexer++) {
		CurrentFunctionGroupNode = &(RootNode->ChildNodes[CurrentFgArrayIndexer]);

		if (CurrentFunctionGroupNode->NodeId == NodeIdToSearch) {
			memcpy(NodeDetected, CurrentFunctionGroupNode, sizeof(struct Node));
			return EFI_SUCCESS;
		}

		CurrentWidgetArrayIndexer = 0;
		for (CurrentWidgetId = CurrentFunctionGroupNode->StartingChildNodeAddess; CurrentWidgetId < CurrentFunctionGroupNode->StartingChildNodeAddess + CurrentFunctionGroupNode->SubordinateNodeCount - 1; CurrentWidgetId++, CurrentWidgetArrayIndexer++) {
			CurrentWidgetNode = &(CurrentFunctionGroupNode->ChildNodes[CurrentWidgetArrayIndexer]);

			if (CurrentWidgetNode->NodeId == NodeIdToSearch) {
				memcpy(NodeDetected, CurrentWidgetNode,sizeof(struct Node));
				return EFI_SUCCESS;
			}
		}
	}

	return EFI_SUCCESS;
}

EFI_STATUS GetCodecTree(PCI_HDA_REGION *PcieDeviceConfigSpace,
						struct Node *RootNode)
{
	struct Node *CurrentFunctionGroupNode = NULL;
	struct Node *CurrentWidgetNode = NULL;

	uint32_t CurrentFunctionGroupId = 0;
	uint32_t CurrentFgArrayIndexer = 0;

	uint32_t CurrentWidgetId = 0;
	uint32_t CurrentWidgetArrayIndexer = 0;

	FillCodecNode(PcieDeviceConfigSpace, 0, HDA_NODE_ROOT, RootNode);
	RootNode->ChildNodes = mem_alloc(sizeof(struct Node) * (RootNode->SubordinateNodeCount - RootNode->StartingChildNodeAddess));
	memset(RootNode->ChildNodes, 0, sizeof(struct Node) * (RootNode->SubordinateNodeCount - RootNode->StartingChildNodeAddess));

	for (CurrentFunctionGroupId = RootNode->StartingChildNodeAddess; CurrentFunctionGroupId < RootNode->StartingChildNodeAddess + RootNode->SubordinateNodeCount; CurrentFunctionGroupId++, CurrentFgArrayIndexer++) {
		CurrentFunctionGroupNode = &(RootNode->ChildNodes[CurrentFgArrayIndexer]);
		FillCodecNode(PcieDeviceConfigSpace, CurrentFunctionGroupId, HDA_NODE_FUNCTION_GROUP, CurrentFunctionGroupNode);
		CurrentFunctionGroupNode->ChildNodes = mem_alloc(sizeof(struct Node) * CurrentFunctionGroupNode->StartingChildNodeAddess + CurrentFunctionGroupNode->SubordinateNodeCount);
		memset(CurrentFunctionGroupNode->ChildNodes, 0, sizeof(struct Node) * CurrentFunctionGroupNode->StartingChildNodeAddess + CurrentFunctionGroupNode->SubordinateNodeCount);

		CurrentWidgetArrayIndexer = 0;
		for (CurrentWidgetId = CurrentFunctionGroupNode->StartingChildNodeAddess; CurrentWidgetId < CurrentFunctionGroupNode->StartingChildNodeAddess + CurrentFunctionGroupNode->SubordinateNodeCount - 1; CurrentWidgetId++, CurrentWidgetArrayIndexer++) {
			CurrentWidgetNode = &(CurrentFunctionGroupNode->ChildNodes[CurrentWidgetArrayIndexer]);
			FillCodecNode(PcieDeviceConfigSpace, CurrentWidgetId, HDA_NODE_WIDGET, CurrentWidgetNode);
		}
	}

	return EFI_SUCCESS;
}

EFI_STATUS ReleaseCodecTree(PCI_HDA_REGION *PcieDeviceConfigSpace,
							struct Node *RootNode)
{
	struct Node *CurrentFunctionGroupNode = NULL;
	uint32_t CurrentFunctionGroupId = 0;
	uint32_t CurrentFgArrayIndexer = 0;

	for (CurrentFunctionGroupId = RootNode->StartingChildNodeAddess; CurrentFunctionGroupId < RootNode->StartingChildNodeAddess + RootNode->SubordinateNodeCount - 1; CurrentFunctionGroupId++, CurrentFgArrayIndexer++) {
		CurrentFunctionGroupNode = &(RootNode->ChildNodes[CurrentFgArrayIndexer]);
		mem_free(CurrentFunctionGroupNode->ChildNodes);
	}

	mem_free(RootNode->ChildNodes);
	return EFI_SUCCESS;
}

EFI_STATUS GetCodecData8BitPayload(PCI_HDA_REGION *PcieDeviceConfigSpace,
			  						uint8_t CodecAddress, uint8_t NodeId,
			  						HDA_VERB Verb, uint8_t VerbPayload,
			  						uint32_t *Response)
{
	return EFI_SUCCESS;
}

EFI_STATUS GetCodecData8BitPayloadCorbRirb(PCI_HDA_REGION *PcieDeviceConfigSpace,
										uint8_t CodecAddress, uint8_t NodeId,
										HDA_VERB Verb, uint8_t VerbPayload,
										uint32_t *Response)
{
	HDA_COMMAND_FIELD_8BIT_PAYLOAD Command;
	HDA_RESPONSE_FIELD *RawResponse;

	uint32_t VerbToSend = 0;
	uint64_t TempResponse = 0;

	Command.CAd = CodecAddress;
	Command.NID = NodeId;
	Command.VerbIdent = Verb;
	Command.VerbPayload = VerbPayload;

	memcpy(&VerbToSend, &Command, sizeof(uint32_t));
	AddCommandToCorb(VerbToSend);

	gBootServices->Stall(200);

	ReadResponseFromRirb(&TempResponse);

	RawResponse = (HDA_RESPONSE_FIELD *)&TempResponse;
	memcpy(Response, &RawResponse->Response, sizeof(uint32_t));
	return EFI_SUCCESS;
}

EFI_STATUS GetCodecData16BitPayloadCorbRirb(PCI_HDA_REGION *PcieDeviceConfigSpace,
										uint8_t CodecAddress, uint8_t NodeId,
										HDA_VERB Verb, uint16_t VerbPayload,
										uint32_t *Response)
{
	HDA_COMMAND_FIELD_16BIT_PAYLOAD Command;
	HDA_RESPONSE_FIELD *RawResponse;

	uint32_t VerbToSend = 0;
	uint64_t TempResponse = 0;

	Command.CAd = CodecAddress;
	Command.NID = NodeId;
	Command.VerbIdent = Verb;
	Command.VerbPayload = VerbPayload;

	memcpy(&VerbToSend, &Command, sizeof(uint32_t));
	AddCommandToCorb(VerbToSend);

	gBootServices->Stall(200);

	ReadResponseFromRirb(&TempResponse);

	RawResponse = (HDA_RESPONSE_FIELD *)&TempResponse;
	memcpy(Response, &RawResponse->Response, sizeof(uint32_t));
	return EFI_SUCCESS;
}


EFI_STATUS SendCommandToAllWidgets8BitPayload (
		PCI_HDA_REGION *PcieDeviceConfigSpace,
		HDA_VERB Verb, uint8_t VerbPayload)
{
	struct Node RootNode;
	uint32_t Response;

	GetCodecTree(PcieDeviceConfigSpace, &RootNode);

	struct Node *CurrentFunctionGroupNode = NULL;
	struct Node *CurrentWidgetNode = NULL;

	uint32_t CurrentFunctionGroupId = 0;
	uint32_t CurrentFgArrayIndexer = 0;

	uint32_t CurrentWidgetId = 0;
	uint32_t CurrentWidgetArrayIndexer = 0;

	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, 0, Verb, VerbPayload, &Response);

	debug("Verb 0x%llx with payload 0x%llx sent to node 0x0\n", Verb, VerbPayload);
	for (CurrentFunctionGroupId = RootNode.StartingChildNodeAddess; CurrentFunctionGroupId < RootNode.StartingChildNodeAddess + RootNode.SubordinateNodeCount; CurrentFunctionGroupId++, CurrentFgArrayIndexer++) {
		GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentFunctionGroupId, Verb, VerbPayload, &Response);
		CurrentFunctionGroupNode = &(RootNode.ChildNodes[CurrentFgArrayIndexer]);
		CurrentWidgetArrayIndexer = 0;

		for (CurrentWidgetId = CurrentFunctionGroupNode->StartingChildNodeAddess; CurrentWidgetId < CurrentFunctionGroupNode->StartingChildNodeAddess + CurrentFunctionGroupNode->SubordinateNodeCount - 1; CurrentWidgetId++, CurrentWidgetArrayIndexer++) {
			CurrentWidgetNode = &(CurrentFunctionGroupNode->ChildNodes[CurrentWidgetArrayIndexer]);
			GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentWidgetId, Verb, VerbPayload, &Response);
		}
	}

	ReleaseCodecTree(PcieDeviceConfigSpace, &RootNode);
	return EFI_SUCCESS;
}

EFI_STATUS SendCommandToAllWidgets16BitPayload (
		PCI_HDA_REGION* PcieDeviceConfigSpace,
		HDA_VERB Verb, uint16_t VerbPayload)
{
	struct Node RootNode;
	uint32_t Response;

	GetCodecTree(PcieDeviceConfigSpace, &RootNode);

	struct Node *CurrentFunctionGroupNode = NULL;
	struct Node *CurrentWidgetNode = NULL;

	uint32_t CurrentFunctionGroupId = 0;
	uint32_t CurrentFgArrayIndexer = 0;

	uint32_t CurrentWidgetId = 0;
	uint32_t CurrentWidgetArrayIndexer = 0;

	GetCodecData16BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, 0, Verb, VerbPayload, &Response);

	for (CurrentFunctionGroupId = RootNode.StartingChildNodeAddess; CurrentFunctionGroupId < RootNode.StartingChildNodeAddess + RootNode.SubordinateNodeCount; CurrentFunctionGroupId++, CurrentFgArrayIndexer++) {
		GetCodecData16BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentFunctionGroupId, Verb, VerbPayload, &Response);
		CurrentFunctionGroupNode = &(RootNode.ChildNodes[CurrentFgArrayIndexer]);
		CurrentWidgetArrayIndexer = 0;

		for (CurrentWidgetId = CurrentFunctionGroupNode->StartingChildNodeAddess; CurrentWidgetId < CurrentFunctionGroupNode->StartingChildNodeAddess + CurrentFunctionGroupNode->SubordinateNodeCount - 1; CurrentWidgetId++, CurrentWidgetArrayIndexer++) {
			CurrentWidgetNode = &(CurrentFunctionGroupNode->ChildNodes[CurrentWidgetArrayIndexer]);
			GetCodecData16BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, CurrentWidgetId, Verb, VerbPayload, &Response);
		}
	}

	ReleaseCodecTree(PcieDeviceConfigSpace, &RootNode);
	return EFI_SUCCESS;
}

EFI_STATUS GetCodecData16BitPayload(PCI_HDA_REGION* PcieDeviceConfigSpace,
			  uint8_t CodecAddress, uint8_t NodeId,
			  HDA_VERB Verb, uint16_t VerbPayload,
			  uint32_t *Response)
{
	return EFI_SUCCESS;
}

uint32_t GetAmplifierGain(PCI_HDA_REGION *PcieDeviceConfigSpace,
						uint8_t NodeId, bool InputOutput,
						bool LeftRight)
{
	uint32_t Response = 0;
	uint16_t PayloadToSend = 0;

	PayloadToSend = NodeId;
	PayloadToSend |= (InputOutput << 15);
	PayloadToSend |= (LeftRight << 13);

	GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0x0, NodeId, HDA_VRB_GET_AMPLIFIER_GAIN_MUTE, 0x0, &Response);
	return Response;
}

EFI_STATUS DisablePcieInterrupts(PCI_HDA_REGION *PcieDeviceConfigSpace)
{
	uint16_t CommandRegister = 0;

	ReadControllerPcieConfiguration(PcieDeviceConfigSpace, HDA_OFFSET_PCIE_PCICMD, &CommandRegister, 1, EfiPciWidthUint16);
	CommandRegister += (1 << 10);
	WriteControllerPcieConfiguration(PcieDeviceConfigSpace, HDA_OFFSET_PCIE_PCICMD, (void *)&CommandRegister, 1, EfiPciWidthUint16);

	return EFI_SUCCESS;
}

EFI_STATUS EnablePcieNoSnoop(PCI_HDA_REGION *PcieDeviceConfigSpace)
{
	uint16_t DeviceControlRegister = 0;
	
	ReadControllerPcieConfiguration(PcieDeviceConfigSpace, HDA_OFFSET_PCIE_DEVCTL, (void *)&DeviceControlRegister, 1, EfiPciWidthUint16);
	DeviceControlRegister += (1 << 11);
	WriteControllerPcieConfiguration(PcieDeviceConfigSpace, HDA_OFFSET_PCIE_DEVCTL, (void *)&DeviceControlRegister, 1, EfiPciWidthUint16);
	
	return EFI_SUCCESS;
}

EFI_STATUS AddDescriptorListEntryOss0(PCI_HDA_REGION *PcieDeviceConfigSpace,
									HDA_CONTROLLER_REGISTER_SET *ControllerRegisterSet,
									uint64_t DataAddress,
									uint32_t DataLength,
									uint8_t BdlEntryIndex,
									uint32_t SdxLastValidIndex)
{
	EFI_UINT64 BdlAddress;
	EFI_UINT32 BdlLower;
	EFI_UINT64 BdlUpper;

	HDA_BUFFER_DESCRIPTOR_LIST *DescriptorList = NULL;

	ReadControllerRegister(PcieDeviceConfigSpace, CALCULATE_OSSN_OFFSET(0, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXBDPL,
						(void *)&BdlLower, 1, EfiPciWidthUint32);
	ReadControllerRegister(PcieDeviceConfigSpace, CALCULATE_OSSN_OFFSET(0, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXBDPU,
						(void *)&BdlUpper, 1, EfiPciWidthUint32);

	BdlAddress = BdlLower | (BdlUpper << 32);

	DescriptorList = (HDA_BUFFER_DESCRIPTOR_LIST *)BdlAddress;
	DescriptorList->BDLEntry[BdlEntryIndex].Address = DataAddress;
	DescriptorList->BDLEntry[BdlEntryIndex].Length = DataLength;

	// TODO: Enable this after Corb/Rirb start to process unsolicited responses
	DescriptorList->BDLEntry[BdlEntryIndex].IntrptOnComp = 1;

	return WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_OSSN_OFFSET(0, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXLVI,
									(void *)&SdxLastValidIndex, 1, Bar0, EfiPciWidthUint32);
}

EFI_STATUS AllocateStreamsPages(PCI_HDA_REGION *PcieDeviceConfigSpace,
								HDA_CONTROLLER_REGISTER_SET *ControllerRegisterSet)
{
	uint8_t *BdlAddressPointer = NULL;
	void *BdlMapping = NULL;
	uint64_t BdlAddress;

	// DMAPositionBuffer
	uint8_t *DmaAddressPointer = NULL;
	void *DmaMapping = NULL;
	uint64_t DmaAddress;

	uint32_t WriteValue = 0;

	uint64_t IssCount = (ControllerRegisterSet->GCAP >> 8) & 0xF;
	uint64_t OssCount = (ControllerRegisterSet->GCAP >> 12) & 0xF;
	uint64_t BssCount = (ControllerRegisterSet->GCAP >> 3) & 0x1F;
	uint64_t Count = 0;

	for (Count = 0; Count < IssCount; Count++) {
		SetupCommonBuffer(&BdlAddressPointer, sizeof(HDA_BUFFER_DESCRIPTOR_LIST), &BdlMapping, 16);
		
		BdlAddress = (uint64_t)BdlAddressPointer;
		WriteValue = (BdlAddress & 0xFFFFFFFF);

		WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_ISSN_OFFSET(Count) + HDA_RELATIVE_OFFSET_SDXBDPL,
								(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
		WriteValue = (BdlAddress >> 32);
		WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_ISSN_OFFSET(Count) + HDA_RELATIVE_OFFSET_SDXBDPU,
								(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
	}

	for (Count = 0; Count < OssCount; Count++) {
		SetupCommonBuffer(&BdlAddressPointer, sizeof(HDA_BUFFER_DESCRIPTOR_LIST), &BdlMapping, 16);
		
		BdlAddress = (uint64_t)BdlAddressPointer;
		WriteValue = (BdlAddress & 0xFFFFFFFF);

		WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_OSSN_OFFSET(Count, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXBDPL,
								(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
		WriteValue = (BdlAddress >> 32);
		WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_OSSN_OFFSET(Count, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXBDPU,
								(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
	}

	for (Count = 0; Count < BssCount; Count++) {
		SetupCommonBuffer(&BdlAddressPointer, sizeof(HDA_BUFFER_DESCRIPTOR_LIST), &BdlMapping, 16);
		
		BdlAddress = (uint64_t)BdlAddressPointer;
		WriteValue = (BdlAddress & 0xFFFFFFFF);

		WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_BSSN_OFFSET(Count, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXBDPL,
								(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
		WriteValue = (BdlAddress >> 32);
		WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_BSSN_OFFSET(Count, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXBDPU,
								(void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
	}

	SetupCommonBuffer(&DmaAddressPointer, (IssCount + OssCount + BssCount) * sizeof(uint64_t), &DmaMapping, 16);
	DmaAddress = (uint64_t)DmaAddressPointer;

	WriteValue = (DmaAddress & 0xFFFFFFFF);
	WriteValue |= 0x01;
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_DPIBLBASE, (void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
	WriteValue = (DmaAddress >> 32);
	WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_DPIBUBASE, (void *)&WriteValue, 1, Bar0, EfiPciWidthUint32);
	
	return EFI_SUCCESS;
}
