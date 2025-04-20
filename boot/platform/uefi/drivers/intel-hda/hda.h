/*********************************************************************************/
/* Module Name:  hda.h                                                           */
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

#ifndef _HDA_HDA_H
#define _HDA_HDA_H

#include <stdint.h>
#include <stdbool.h>

// Intel HDA Controller location
#define HDA_BUS 0
#define HDA_DEV 27
#define HDA_FUNC 0

// HDA Spec page 57
#define HDA_BUFFER_DESC_LIST_MAX_ENTRIES 256

// IO-Controller_Hub-7-hd-audio-ac97 manual, page 13
typedef struct {
	uint16_t VID;
	uint16_t DID;
	uint16_t PCICMD;
	uint16_t PCISTS;
	uint8_t RID;
	uint8_t PI;
	uint8_t SC;
	uint8_t BCC;
	uint8_t CLS;
	uint8_t LT;
	uint8_t HEADTYP;
	uint8_t RES0;
	uint32_t HDBARL;
	uint32_t HDBARU;
	uint8_t RESV1[20];
	uint16_t SVID;
	uint16_t SID;
	uint8_t RESV2[4];
	uint8_t CAPPTR;
	uint8_t RESV3[7];
	uint8_t INTLN;
	uint8_t INTPN;
	uint8_t RESV4[2];
	uint8_t HDCTL;
	uint8_t RESV5[3];
	uint8_t TCSEL;
	uint8_t RESV6[8];
	uint8_t DCKSTS;
	uint8_t RESV7[2];
	uint16_t PID;
	uint16_t PC;
	uint32_t PCS;
	uint8_t RESV8[8];
	uint16_t MID;
	uint16_t MMC;
	uint32_t MMLA;
	uint32_t MMUA;
	uint16_t MMD;
	uint8_t RESV9[2];
	uint16_t PXID;
	uint16_t PXC;
	uint32_t DEVCAP;
	uint16_t DEVCTL;
	uint16_t DEVS;
	uint8_t RESV10[132];
	uint32_t VCCAP;
	uint32_t PVCCAP1;
	uint32_t PVCCAP2;
	uint16_t PVCCTL;
	uint16_t PVCSTS;
	uint32_t VC0CAP;
	uint32_t VC0CTL;
	uint8_t RESV11[2];
	uint16_t VC0STS;
	uint32_t VciCAP;
	uint32_t VciCTL;
	uint8_t RESV12[2];
	uint16_t VciSTS;
	uint8_t RESV13[8];
	uint32_t RCCAP;
	uint32_t ESD;
	uint8_t RESV14[8];
	uint32_t L1DESC;
	uint8_t RESV15[4];
	uint32_t L1ADDL;
	uint32_t L1ADDU;
} PCI_HDA_REGION;

// Register offsets defined at HDA PCIe config space
#define HDA_OFFSET_PCIE_VID 0x0
#define HDA_OFFSET_PCIE_DID 0x2
#define HDA_OFFSET_PCIE_PCICMD 0x4
#define HDA_OFFSET_PCIE_PCISTS 0x6
#define HDA_OFFSET_PCIE_RID 0x8
#define HDA_OFFSET_PCIE_PI 0x9
#define HDA_OFFSET_PCIE_SCC 0xA
#define HDA_OFFSET_PCIE_BCC 0xB
#define HDA_OFFSET_PCIE_CLS 0xC
#define HDA_OFFSET_PCIE_LT 0xD
#define HDA_OFFSET_PCIE_HEADTYP 0xE
#define HDA_OFFSET_PCIE_HDBARL 0x10
#define HDA_OFFSET_PCIE_HDBARU 0x14
#define HDA_OFFSET_PCIE_SVID 0x2C
#define HDA_OFFSET_PCIE_SID 0x2E
#define HDA_OFFSET_PCIE_CAPPTR 0x34
#define HDA_OFFSET_PCIE_INTLN 0x3C
#define HDA_OFFSET_PCIE_INTPN 0x3D
#define HDA_OFFSET_PCIE_HDCTL 0x40
#define HDA_OFFSET_PCIE_TCSEL 0x44
#define HDA_OFFSET_PCIE_DCKSTS 0x4D
#define HDA_OFFSET_PCIE_PID 0x50
#define HDA_OFFSET_PCIE_PC 0x52
#define HDA_OFFSET_PCIE_PCS 0x54
#define HDA_OFFSET_PCIE_MID 0x60
#define HDA_OFFSET_PCIE_MMC 0x62
#define HDA_OFFSET_PCIE_MMLA 0x64
#define HDA_OFFSET_PCIE_MMUA 0x68
#define HDA_OFFSET_PCIE_MMD 0x6C
#define HDA_OFFSET_PCIE_PXID 0x70
#define HDA_OFFSET_PCIE_PXC 0x72
#define HDA_OFFSET_PCIE_DEVCAP 0x74
#define HDA_OFFSET_PCIE_DEVCTL 0x78
#define HDA_OFFSET_PCIE_DEVS 0x7A
#define HDA_OFFSET_PCIE_VCCAP 0x100
#define HDA_OFFSET_PCIE_PVCCAP1 0x104
#define HDA_OFFSET_PCIE_PVCCAP2 0x108
#define HDA_OFFSET_PCIE_PVCCTL 0x10C
#define HDA_OFFSET_PCIE_PVCSTS 0x10E
#define HDA_OFFSET_PCIE_VC0CAP 0x110
#define HDA_OFFSET_PCIE_VC0CTL 0x114
#define HDA_OFFSET_PCIE_VC0STS 0x11A
#define HDA_OFFSET_PCIE_VciCAP 0x11C
#define HDA_OFFSET_PCIE_VciCTL 0x120
#define HDA_OFFSET_PCIE_VciSTS 0x126
#define HDA_OFFSET_PCIE_RCCAP 0x130
#define HDA_OFFSET_PCIE_ESD 0x134
#define HDA_OFFSET_PCIE_L1DESC 0x140
#define HDA_OFFSET_PCIE_L1ADDL 0x148
#define HDA_OFFSET_PCIE_L1ADDU 0x14C

/* Offsets of the registers defined at HDA
 * compatible controllers.
 * Details can be found at the page 27 of the
 * HDA Specification Rev 1.0a
*/
// Register offsets defined at HDA compatible controllers
#define HDA_OFFSET_GCAP	0x0
#define HDA_OFFSET_VMIN	0x2
#define HDA_OFFSET_VMAJ	0x3
#define HDA_OFFSET_OUTPAY 0x4
#define HDA_OFFSET_INPAY 0x6
#define HDA_OFFSET_GCTL	0x8
#define HDA_OFFSET_WAKEEN 0x0C
#define HDA_OFFSET_WAKESTS 0x0E
#define HDA_OFFSET_GSTS	0x10
#define HDA_OFFSET_Rsvd	0x12
#define HDA_OFFSET_OUTSTRMPAY 0x18
#define HDA_OFFSET_INSTRMPAY 0x1A
#define HDA_OFFSET_INTCTL 0x20
#define HDA_OFFSET_INTSTS 0x24
#define HDA_OFFSET_WALCLK 0x30
#define HDA_OFFSET_SSYNC 0x38
#define HDA_OFFSET_CORBLBASE 0x40
#define HDA_OFFSET_CORBUBASE 0x44
#define HDA_OFFSET_CORBWP 0x48
#define HDA_OFFSET_CORBRP 0x4A
#define HDA_OFFSET_CORBCTL 0x4C
#define HDA_OFFSET_CORBSTS 0x4D
#define HDA_OFFSET_CORBSIZE 0x4E
#define HDA_OFFSET_RIRBLBASE 0x50
#define HDA_OFFSET_RIRBUBASE 0x54
#define HDA_OFFSET_RIRBWP 0x58
#define HDA_OFFSET_RINTCNT 0x5A
#define HDA_OFFSET_RIRBCTL 0x5C
#define HDA_OFFSET_RIRBSTS 0x5D
#define HDA_OFFSET_RIRBSIZE 0x5E
#define HDA_OFFSET_ICOI	0x60
#define HDA_OFFSET_ICII	0x64
#define HDA_OFFSET_ICIS	0x68
#define HDA_OFFSET_DPIBLBASE 0x70
#define HDA_OFFSET_DPIBUBASE 0x74

#define HDA_OFFSET_SD0CTL 0x80
#define HDA_OFFSET_SD0STS 0x83
#define HDA_OFFSET_SD0LPIB 0x84
#define HDA_OFFSET_SD0CBL 0x88
#define HDA_OFFSET_SD0LVI 0x8C
#define HDA_OFFSET_SD0FIFOS 0x90
#define HDA_OFFSET_SD0FMT 0x92
#define HDA_OFFSET_SD0BDPL 0x98
#define HDA_OFFSET_SD0BDPU 0x9C

// HDA Spec 1.0a page 27
#define HDA_RELATIVE_OFFSET_SDXCTL 0
#define HDA_RELATIVE_OFFSET_SDXSTS 0x3
#define HDA_RELATIVE_OFFSET_SDXLPIB 0x4
#define HDA_RELATIVE_OFFSET_SDXCBL 0x8
#define HDA_RELATIVE_OFFSET_SDXLVI 0xC
#define HDA_RELATIVE_OFFSET_SDXFIFOS 0x10
#define HDA_RELATIVE_OFFSET_SDXFMT 0x12
#define HDA_RELATIVE_OFFSET_SDXBDPL 0x18
#define HDA_RELATIVE_OFFSET_SDXBDPU 0x1C

// Extract OSS count from controller's global capabilities register
#define HDA_OSS_COUNT(GCAP) ((GCAP >> 12) & 0xF)

// Extract ISS count from controller's global capabilities register
#define HDA_ISS_COUNT(GCAP) ((GCAP >> 8) & 0xF)

// Extract BSS count from controller's global capabilities register
#define HDA_BSS_COUNT(GCAP) ((GCAP >> 3) & 0x1F)

///
// Calculate offset of stream descriptors
#define CALCULATE_ISSN_OFFSET(StreamIndex) (HDA_OFFSET_SD0CTL + (StreamIndex * 0x20))
#define CALCULATE_OSSN_OFFSET(StreamIndex,GCAP) (HDA_OFFSET_SD0CTL + (HDA_ISS_COUNT(GCAP) * 0x20 ) + (StreamIndex * 0x20))
#define CALCULATE_BSSN_OFFSET(StreamIndex,GCAP) (HDA_OFFSET_SD0CTL + (HDA_ISS_COUNT(GCAP) * 0x20) + (HDA_OSS_COUNT(GCAP) * 0x20) + (StreamIndex * 0x20))

typedef struct {
	uint8_t StreamReset: 1;
	uint8_t StreamRun: 1;
	uint8_t InterruptOnCompletionEnable: 1;
	uint8_t FIFOErrorInterruptEnable: 1;
	uint8_t DescriptorErrorInterruptEnable: 1;
	uint8_t Reserved1: 3;
	uint8_t Reserved2;
	uint8_t StrippeControl: 2;
	uint8_t TrafficPriority: 1;
	uint8_t BidirectionalDirectionControl: 1;
	uint8_t StreamNumber: 4;
} HDA_CONTROLLER_STREAM_DESCRIPTOR_CONTROL;

typedef struct {
	uint8_t SD0CTL[3]; // HDA_CONTROLLER_STREAM_DESCRIPTOR_CONTROL
	uint8_t SD0STS;
	uint32_t SD0LPIB;
	uint32_t SD0CBL;
	uint16_t SD0LVI;
	uint16_t Rsvd8;
	uint16_t SD0FIFOS;
	uint16_t SD0FMT;
	uint32_t Rsvd9;
	uint32_t SD0BDPL;
	uint32_t SD0BDPU;
} HDA_CONTROLLER_STREAM_DESCRIPTOR;

// HDA controller register set
// HDA Specification 1.0a page 27
typedef struct {
	uint16_t GCAP;
	uint8_t VMIN;
	uint8_t VMAJ;
	uint16_t OUTPAY;
	uint16_t INPAY;
	uint32_t GCTL;
	uint16_t WAKEEN;
	uint16_t STATESTS;
	uint16_t GSTS;
	uint8_t Rsvd0[6];
	uint16_t OUTSTRMPAY;
	uint16_t INSTRMPAY;
	uint32_t Rsvd;
	uint32_t INTCTL;
	uint32_t INTSTS;
	uint8_t Rsvd1[8];
	uint32_t WALCLK;
	uint32_t Rsvd2;
	uint32_t SSYNC;
	uint32_t Rsvd3;
	uint32_t CORBLBASE;
	uint32_t CORBUBASE;
	uint16_t CORBWP;
	uint16_t CORBRP;
	uint8_t CORBCTL;
	uint8_t CORBSTS;
	uint8_t CORBSIZE;
	uint8_t Rsvd4;
	uint32_t RIRBLBASE;
	uint32_t RIRBUBASE;
	uint16_t RIRBWP;
	uint16_t RINTCNT;
	uint8_t RIRBCTL;
	uint8_t RIRBSTS;
	uint8_t RIRBSIZE;
	uint8_t Rsvd5;
	uint32_t ICOI;
	uint32_t ICII;
	uint16_t ICIS;
	uint8_t Rsvd6[6];
	uint32_t DPIBLBASE;
	uint32_t DPIBUBASE;
	uint8_t Rsvd7[8];

	HDA_CONTROLLER_STREAM_DESCRIPTOR *ISS;
	HDA_CONTROLLER_STREAM_DESCRIPTOR *OSS;
	HDA_CONTROLLER_STREAM_DESCRIPTOR *BSS;

} HDA_CONTROLLER_REGISTER_SET;

// Executable verbs of widgets on a HDA compatible codec
// HDA Specification 1.0a page 218
typedef enum {
	HDA_VRB_GET_PARAMETER = 0xF00,
	HDA_VRB_GET_CONNECTION_SELECT = 0xF01,
	HDA_VRB_GET_GET_CONNECTION_LIST_ENTRY = 0xF02,
	HDA_VRB_GET_PROCESSING_STATE = 0xF03,

	// These are widget dependent
	HDA_VRB_GET_COEFFICIENT_INDEX = 0xD,
	HDA_VRB_GET_PROCESSING_COEFFICIENT = 0xC,
	HDA_VRB_GET_AMPLIFIER_GAIN_MUTE = 0xBA0,
	HDA_VRB_GET_STREAM_FORMAT = 0xA,

	HDA_VRB_GET_DIGITAL_CONVERTER_1 = 0xF0D,
	//HDA_VRB_GET_DIGITAL_CONVERTER_2 = 0xF0D,
	//HDA_VRB_GET_DIGITAL_CONVERTER_3 = 0xF0D,
	//HDA_VRB_GET_DIGITAL_CONVERTER_4 = 0xF0D,

	HDA_VRB_GET_POWER_STATE = 0xF05,
	HDA_VRB_GET_CHANNEL_STREAM_ID = 0xF06,
	HDA_VRB_GET_SDI_SELECT = 0xF04,
	HDA_VRB_GET_PIN_WIDGET_CONTROL = 0xF07,
	HDA_VRB_GET_UNSOLICITED_ENABLE = 0xF08,
	HDA_VRB_GET_PIN_SENSE = 0xF09,
	HDA_VRB_GET_EAPD_BTL_ENABLE = 0xF0C,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F10 = 0xF10,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F11 = 0xF11,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F12 = 0xF12,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F13 = 0xF13,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F14 = 0xF14,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F15 = 0xF15,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F16 = 0xF16,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F17 = 0xF17,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F18 = 0xF18,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F19 = 0xF19,
	HDA_VRB_GET_ALL_GPI_CONTROLS_F1A = 0xF1A,
	HDA_VRB_GET_BEEP_GENERATION_CONTROL = 0xF0A,
	HDA_VRB_GET_VOLUME_KNOB_CONTROL = 0xF0F,
	HDA_VRB_GET_IMPLEMENTATION_ID_BYTE_0 = 0xF20,
	//HDA_VRB_GET_IMPLEMENTATION_ID_BYTE_1 = 0xF20,
	//HDA_VRB_GET_IMPLEMENTATION_ID_BYTE_2 = 0xF20,
	//HDA_VRB_GET_IMPLEMENTATION_ID_BYTE_3 = 0xF20,
	HDA_VRB_GET_CONFIG_DEFAULT_BYTE_0 = 0xF1C,
	//HDA_VRB_GET_CONFIG_DEFAULT_BYTE_1 = 0xF1C,
	//HDA_VRB_GET_CONFIG_DEFAULT_BYTE_2 = 0xF1C,
	//HDA_VRB_GET_CONFIG_DEFAULT_BYTE_3 = 0xF1C,
	HDA_VRB_GET_STRIPE_CONTROL = 0xF24,
	HDA_VRB_GET_CONVERTER_CHANNEL_COUNT = 0xF2D,
	HDA_VRB_GET_DIP_SIZE = 0xF2E,
	HDA_VRB_GET_ELD_DATA = 0xF2F,
	HDA_VRB_GET_DIP_INDEX = 0xF30,
	HDA_VRB_GET_DIP_DATA = 0xF31,
	HDA_VRB_GET_DIP_XMITCTRL = 0xF32,
	HDA_VRB_GET_CONTENT_PROTECTION_CONTROL = 0xF33,
	HDA_VRB_GET_ASP_CHANNEL_MAPPING = 0xF34,

	HDA_VRB_SET_CONNECTION_SELECT = 0x701,

	// These are widget dependent
	HDA_VRB_SET_COEFFICIENT_INDEX = 0x5,
	HDA_VRB_SET_PROCESSING_COEFFICIENT = 0x4,
	HDA_VRB_SET_AMPLIFIER_GAIN_MUTE = 0x3B0,
	HDA_VRB_SET_STREAM_FORMAT = 0x2,

	HDA_VRB_SET_DIGITAL_CONVERTER_1 = 0x70D,
	HDA_VRB_SET_DIGITAL_CONVERTER_2 = 0x70E,
	HDA_VRB_SET_DIGITAL_CONVERTER_3 = 0x73E,
	HDA_VRB_SET_DIGITAL_CONVERTER_4 = 0x73F,
	HDA_VRB_SET_POWER_STATE = 0x705,
	HDA_VRB_SET_CHANNEL_STREAM_ID = 0x706,
	HDA_VRB_SET_SDI_SELECT = 0x704,
	HDA_VRB_SET_PIN_WIDGET_CONTROL = 0x707,
	HDA_VRB_SET_UNSOLICITED_ENABLE = 0x708,
	HDA_VRB_SET_PIN_SENSE = 0x709,
	HDA_VRB_SET_EAPD_BTL_ENABLE = 0x70C,
	HDA_VRB_SET_ALL_GPI_CONTROLS_710 = 0x710,
	HDA_VRB_SET_ALL_GPI_CONTROLS_711 = 0x711,
	HDA_VRB_SET_ALL_GPI_CONTROLS_712 = 0x712,
	HDA_VRB_SET_ALL_GPI_CONTROLS_713 = 0x713,
	HDA_VRB_SET_ALL_GPI_CONTROLS_714 = 0x714,
	HDA_VRB_SET_ALL_GPI_CONTROLS_715 = 0x715,
	HDA_VRB_SET_ALL_GPI_CONTROLS_716 = 0x716,
	HDA_VRB_SET_ALL_GPI_CONTROLS_717 = 0x717,
	HDA_VRB_SET_ALL_GPI_CONTROLS_718 = 0x718,
	HDA_VRB_SET_ALL_GPI_CONTROLS_719 = 0x719,
	HDA_VRB_SET_ALL_GPI_CONTROLS_71A = 0x71A,
	HDA_VRB_SET_BEEP_GENERATION_CONTROL = 0x70A,
	HDA_VRB_SET_VOLUME_KNOB_CONTROL = 0x70F,
	HDA_VRB_SET_IMPLEMENTATION_ID_BYTE_0 = 0x720,
	HDA_VRB_SET_IMPLEMENTATION_ID_BYTE_1 = 0x721,
	HDA_VRB_SET_IMPLEMENTATION_ID_BYTE_2 = 0x722,
	HDA_VRB_SET_IMPLEMENTATION_ID_BYTE_3 = 0x723,
	HDA_VRB_SET_CONFIG_DEFAULT_BYTE_0 = 0x71C,
	HDA_VRB_SET_CONFIG_DEFAULT_BYTE_1 = 0x71D,
	HDA_VRB_SET_CONFIG_DEFAULT_BYTE_2 = 0x71E,
	HDA_VRB_SET_CONFIG_DEFAULT_BYTE_3 = 0x723,
	HDA_VRB_SET_STRIPE_CONTROL = 0x724,
	HDA_VRB_SET_CONVERTER_CHANNEL_COUNT = 0x72D,
	HDA_VRB_SET_DIP_INDEX = 0x730,
	HDA_VRB_SET_DIP_DATA = 0x731,
	HDA_VRB_SET_DIP_XMITCTRL = 0x732,
	HDA_VRB_SET_CONTENT_PROTECTION_CONTROL = 0x733,
	HDA_VRB_SET_ASP_CHANNEL_MAPPING = 0x734,

	HDA_VRB_SET_RESET = 0x7FF
} HDA_VERB;

#define HDA_START_PROCESSING_IMMEDIATE_COMMAND 0x01

// Extract the total node count from a SubordinateNodeCount property
#define HDA_SUB_NODE_COUNT_TOTAL_NODE(Subordinate) (Subordinate & 0xFF)

// Extract the total node count from a SubordinateNodeCount poperty
#define HDA_SUB_NODE_COUNT_START_NODE(Subordinate) ((Subordinate >> 16) & 0xFF)

// Extract the node type from a FunctionGroup property
#define HDA_NODE_TYPE(FunctionGroupType) (FunctionGroupType & 0xFF)

// Check if a given function group can generate unsolicited responses
#define HDA_UNSOLICITED_RESPONSE_CAPABLE(FunctionGroupType) ((FunctionGroupType >> 8) & 0x01)

// Extract widget type from widget capabilities information
#define HDA_WIDGET_TYPE(WidgetCap) ((WidgetCap >> 20) & 0xF)

// HDA command with an 8-bit payload
typedef struct {
	uint32_t VerbPayload : 8;
	uint32_t VerbIdent : 12; // verb
	uint32_t NID : 8; // node ID
	uint32_t CAd : 4; // codec address
	uint8_t Reserved;
} HDA_COMMAND_FIELD_8BIT_PAYLOAD;

// HDA command with an 16-bit payload
typedef struct {
	uint32_t VerbPayload : 16;
	uint32_t VerbIdent : 4; // verb
	uint32_t NID : 8; // node ID
	uint32_t CAd : 4; // codec address
	uint8_t Reserved;
} HDA_COMMAND_FIELD_16BIT_PAYLOAD;

// Stream format
// HDA Specification 1.0a page 58
typedef struct {
	uint16_t NumberOfChannels : 4;
	uint16_t BitsPerSample : 3;
	uint16_t Reserved : 1;
	uint16_t SampleBaseRateBaseDivisor : 3;
	uint16_t SampleBaseRateMultiple : 3;
	uint16_t SampleBaseRate : 1;
	uint16_t StreamType : 1;
} HDA_STREAM_FORMAT;

// Stream descriptor format
// HDA Specification 1.0a page 48
typedef struct {
	uint16_t NumberOfChannels : 4;
	uint16_t BitsPerSample : 3;
	uint16_t Reserved : 1;
	uint16_t SampleBaseRateBaseDivisor : 3;
	uint16_t SampleBaseRateMultiple : 3;
	uint16_t SampleBaseRate : 1;
	uint16_t Reserved2 : 1;
} HDA_STREAM_DESCRIPTOR_FORMAT;

#define PCM_STRUCT_BITS_PER_SAMPLE_8 0
#define PCM_STRUCT_BITS_PER_SAMPLE_16 1
#define PCM_STRUCT_BITS_PER_SAMPLE_20 2
#define PCM_STRUCT_BITS_PER_SAMPLE_24 3
#define PCM_STRUCT_BITS_PER_SAMPLE_32 4

#define PCM_STRUCT_SAMPLE_BASE_DIV_BY_1 0
#define PCM_STRUCT_SAMPLE_BASE_DIV_BY_2 1
#define PCM_STRUCT_SAMPLE_BASE_DIV_BY_3 2
#define PCM_STRUCT_SAMPLE_BASE_DIV_BY_4 3
#define PCM_STRUCT_SAMPLE_BASE_DIV_BY_5 4
#define PCM_STRUCT_SAMPLE_BASE_DIV_BY_6 5
#define PCM_STRUCT_SAMPLE_BASE_DIV_BY_7 6

//192 kHz, 176.4 kHz
#define PCM_STRUCT_SAMPLE_BASE_MULTIPLE_X4 3
//144 kHz
#define PCM_STRUCT_SAMPLE_BASE_MULTIPLE_X3 2
//96 kHz, 88.2 kHz, 32 kHz
#define PCM_STRUCT_SAMPLE_BASE_MULTIPLE_X2 1
//48KHz/44.1kHz or less
#define PCM_STRUCT_SAMPLE_BASE_MULTIPLE_48_OR_LESS 0

#define PCM_STRUCT_SAMPLE_BASE_44_1KHZ 1
#define PCM_STRUCT_SAMPLE_BASE_48KHZ 0

#define PCM_STRUCT_TYPE_PCM 0
#define PCM_STRUCT_TYPE_NON_PCM 1

// All possible parameters for executing HDA_VRB_GET_PARAMETER
// HDA Specification 1.0a page 217
typedef enum {
	HDA_PARAM_VENDOR_ID = 0x00,
	HDA_PARAM_REVISION_ID = 0x02,
	HDA_PARAM_SUBORDINATE_NODE_COUNT = 0x04,
	HDA_PARAM_FUNCTION_GROUP_TYPE = 0x05,
	HDA_PARAM_AUDIO_FUNC_CAP = 0x08,
	HDA_PARAM_AUDIO_WIDGET_CAP = 0x09,
	HDA_PARAM_SAMPLE_SIZE_RATE_CAP = 0x0A,
	HDA_PARAM_STREAM_FORMATS = 0x0B,
	HDA_PARAM_PIN_CAP = 0x0C,
	HDA_PARAM_INPUT_AMP_CAP = 0x0D,
	HDA_PARAM_OUTPUT_AMP_CAP = 0x12,
	HDA_PARAM_CONNECTION_LIST_LENGTH = 0x0E,
	HDA_PARAM_SUPPORTED_POWER_STATES = 0x0F,
	HDA_PARAM_PROCESSING_CAP = 0x10,
	HDA_PARAM_GPIO_COUNT = 0x11,
	HDA_PARAM_VOLUME_KNOB_CAP = 0x13
} HDA_PARAMETER;

// All possible parameters widget types present
typedef enum {
	HDA_WIDGET_TYPE_AUDIO_OUTPUT = 0x0,
	HDA_WIDGET_TYPE_AUDIO_INPUT = 0x1,
	HDA_WIDGET_TYPE_AUDIO_MIXER = 0x2,
	HDA_WIDGET_TYPE_AUDIO_SELECTOR = 0x3,
	HDA_WIDGET_TYPE_AUDIO_PIN_CONPLEX = 0x4,
	HDA_WIDGET_TYPE_AUDIO_POWER = 0x5,
	HDA_WIDGET_TYPE_AUDIO_VOLUME_KNOB = 0x6,
	HDA_WIDGET_TYPE_AUDIO_BEEP_GENERATOR = 0x7,
	HDA_WIDGET_TYPE_AUDIO_VENDOR_DEFINED = 0xF
} HDA_WIDGET_TYPE;

// Function group types
typedef enum {
	HDA_FUNCTION_GROUP_TYPE_AUDIO = 0x1,
	HDA_FUNCTION_GROUP_TYPE_MODEM = 0x2
} HDA_FUNCTION_GROUP_TYPE;

// All possible node types available at a codec
typedef enum {
	HDA_NODE_ROOT = 0x0,
	HDA_NODE_FUNCTION_GROUP = 0x1,
	HDA_NODE_WIDGET = 0x2,
	HDA_UNKNOWN = 0xFF
} HDA_NODE_TYPE;

// Empty payload
#define HDA_VRB_EMPTY_PAYLOAD 0

// All possible power stats a node can present
// HDA Specification 1.0a page 151
typedef enum {
	HDA_POWER_D0 = 0x0,
	HDA_POWER_D1 = 0x1,
	HDA_POWER_D2 = 0x2,
	HDA_POWER_D3 = 0x3,
	HDA_POWER_D3_COLD = 0x4,
} HDA_POWER_STATE;

// HDA codec node
struct Node {
	uint32_t NodeId;
	HDA_NODE_TYPE NodeType;
	HDA_WIDGET_TYPE WidgetType;
	uint32_t VendorId;
	uint32_t RevisionId;
	uint32_t StartingChildNodeAddess;
	uint32_t SubordinateNodeCount;
	uint32_t FunctionGroupType;
	uint32_t FuncCap;
	uint32_t WidgetCap;
	uint32_t SampleSizeRateCap;
	uint32_t StreamFormat;
	uint32_t PinCap;
	uint32_t InputAmpCap;
	uint32_t OutputAmpCap;
	uint32_t ConnectionListLength;
	uint32_t SupportedPowerStates;
	uint32_t ProcessingCap;
	uint32_t GPIOCount;
	uint32_t VolKnobCap;

	uint32_t PowerState;

	uint32_t RightGain;
	uint32_t LeftGain;

	uint32_t ChannelStreamId;

	struct Node *ChildNodes;
};

// HDA Specification 1.0a page 142
typedef struct {
	uint32_t Response;
	uint8_t Reserved : 2;
	uint8_t UnSol : 1;
	uint8_t Valid : 1;
	uint8_t Unused : 4;
} HDA_RESPONSE_FIELD;

typedef struct {
	uint64_t Address;
	uint32_t Length;
	uint32_t IntrptOnComp : 1;
	uint32_t Resv : 31;
} HDA_BUFFER_DESCRIPTOR_LIST_ENTRY;

typedef struct {
	HDA_BUFFER_DESCRIPTOR_LIST_ENTRY BDLEntry[HDA_BUFFER_DESC_LIST_MAX_ENTRIES];
} HDA_BUFFER_DESCRIPTOR_LIST;

EFI_STATUS InitHda();

EFI_STATUS AllocateCORBBuffer(PCI_HDA_REGION* PcieDeviceConfigSpace);
EFI_STATUS AllocateRIRBBuffer(PCI_HDA_REGION* PcieDeviceConfigSpace);

EFI_STATUS FillCodecNode(PCI_HDA_REGION *PcieDeviceConfigSpace,
						  uint32_t CurrentNodeId,
						  HDA_NODE_TYPE NodeType,
						  struct Node *CurrentNode);

EFI_STATUS GetNodeById(struct Node *RootNode,
					   uint32_t NodeIdToSearch,
					   struct Node *NodeDetected);

EFI_STATUS GetCodecTree(PCI_HDA_REGION *PcieDeviceConfigSpace,
						struct Node *RootNode);

EFI_STATUS ReleaseCodecTree(PCI_HDA_REGION *PcieDeviceConfigSpace,
							struct Node *RootNode);

EFI_STATUS GetCodecData8BitPayload(PCI_HDA_REGION *PcieDeviceConfigSpace,
			  						uint8_t CodecAddress, uint8_t NodeId,
			  						HDA_VERB Verb, uint8_t VerbPayload,
			  						uint32_t *Response);

EFI_STATUS GetCodecData8BitPayloadCorbRirb(PCI_HDA_REGION *PcieDeviceConfigSpace,
										uint8_t CodecAddress, uint8_t NodeId,
										HDA_VERB Verb, uint8_t VerbPayload,
										uint32_t *Response);

EFI_STATUS GetCodecData16BitPayloadCorbRirb(PCI_HDA_REGION *PcieDeviceConfigSpace,
										uint8_t CodecAddress, uint8_t NodeId,
										HDA_VERB Verb, uint16_t VerbPayload,
										uint32_t *Response);


EFI_STATUS SendCommandToAllWidgets8BitPayload (
		PCI_HDA_REGION *PcieDeviceConfigSpace,
		HDA_VERB Verb, uint8_t VerbPayload);

EFI_STATUS SendCommandToAllWidgets16BitPayload (
		PCI_HDA_REGION* PcieDeviceConfigSpace,
		HDA_VERB Verb, uint16_t VerbPayload);

EFI_STATUS GetCodecData16BitPayload(PCI_HDA_REGION* PcieDeviceConfigSpace,
			  uint8_t CodecAddress, uint8_t NodeId,
			  HDA_VERB Verb, uint16_t VerbPayload,
			  uint32_t *Response);

uint32_t GetAmplifierGain(PCI_HDA_REGION *PcieDeviceConfigSpace,
						uint8_t NodeId, bool InputOutput,
						bool LeftRight);

EFI_STATUS DisablePcieInterrupts(PCI_HDA_REGION *PcieDeviceConfigSpace);

EFI_STATUS EnablePcieNoSnoop(PCI_HDA_REGION *PcieDeviceConfigSpace);

EFI_STATUS AddDescriptorListEntryOss0(PCI_HDA_REGION *PcieDeviceConfigSpace,
									HDA_CONTROLLER_REGISTER_SET *ControllerRegisterSet,
									uint64_t DataAddress,
									uint32_t DataLength,
									uint8_t BdlEntryIndex,
									uint32_t SdxLastValidIndex);

EFI_STATUS AllocateStreamsPages(PCI_HDA_REGION *PcieDeviceConfigSpace,
								HDA_CONTROLLER_REGISTER_SET *ControllerRegisterSet);

#endif /* _HDA_HDA_H */
