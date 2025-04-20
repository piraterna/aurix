/*********************************************************************************/
/* Module Name:  entry.c                                                         */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#include <efi.h>
#include <efilib.h>

#include <axboot.h>
#include <mm/mman.h>
#include <lib/string.h>
#include <print.h>

#include <stddef.h>

#include <sounds/chime.h>
#include <hda.h>
#include <pciconf.h>

EFI_HANDLE gImageHandle;
EFI_SYSTEM_TABLE *gSystemTable;
EFI_BOOT_SERVICES *gBootServices;

#define INITIAL_VOLUME 12

uint8_t *AlignedDataBufferBdlEntry = NULL;
uint8_t *SoundData = NULL;
uint64_t SoundDataSize = 0;

EFI_STATUS InitHdaControllerCodecAndBuffers(PCI_HDA_REGION *PcieDeviceConfigSpace, HDA_CONTROLLER_REGISTER_SET *ControllerRegisterSet)
{
    uint32_t WriteValue = 0;
    uint32_t Response = 0;

    DisablePcieInterrupts(PcieDeviceConfigSpace);
    EnablePcieNoSnoop(PcieDeviceConfigSpace);
    
    // traffic priority -> TC0
    WriteValue = 0;
    WriteControllerRegister(PcieDeviceConfigSpace, HDA_OFFSET_PCIE_TCSEL, (void *)&WriteValue, 1, 0, EfiPciWidthUint8);
    
    AllocateRIRBBuffer(PcieDeviceConfigSpace);
    AllocateCORBBuffer(PcieDeviceConfigSpace);
    
    // Turn all nodes on
    SendCommandToAllWidgets8BitPayload(PcieDeviceConfigSpace, HDA_VRB_SET_POWER_STATE, 0x0);

    // Set initial volume to output widgets
    GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0x0, 0x10, HDA_VRB_SET_AMPLIFIER_GAIN_MUTE, INITIAL_VOLUME, &Response);
    GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0x0, 0x11, HDA_VRB_SET_AMPLIFIER_GAIN_MUTE, INITIAL_VOLUME, &Response);
    return EFI_SUCCESS;
}

EFI_STATUS AllocateResourcesBasedOnFile(PCI_HDA_REGION *PcieDeviceConfigSpace, HDA_CONTROLLER_REGISTER_SET *ControllerRegisterSet, EFI_UINTN FileSize, EFI_UINTN DataAddress)
{
    uint8_t WriteValue8 = 0;
    uint16_t WriteValue16 = 0;
    uint32_t WriteValue = 0;
    uint32_t Response = 0;

    void *AlignedDataMapping = NULL;

    uint64_t BdlEntriesRequired = 0;
    uint64_t BdlEntriesRequiredCurrentEntry = 0;

    AllocateStreamsPages(PcieDeviceConfigSpace, ControllerRegisterSet);
    if (SoundDataSize < 0xFFFFFFFF) {
        BdlEntriesRequired = 1;
    } else {
        BdlEntriesRequired = SoundDataSize / 0xFFFFFFFF;
        if (SoundDataSize % 0xFFFFFFFF > 0) {
            BdlEntriesRequired += 1;
        }
    }

    for (BdlEntriesRequiredCurrentEntry = 0; BdlEntriesRequiredCurrentEntry < 2; BdlEntriesRequiredCurrentEntry++) {
        SetupCommonBuffer(&AlignedDataBufferBdlEntry, SoundDataSize / BdlEntriesRequired, &AlignedDataMapping, 2);

        memcpy(AlignedDataBufferBdlEntry, SoundData, SoundDataSize / BdlEntriesRequired);
        AddDescriptorListEntryOss0(PcieDeviceConfigSpace, ControllerRegisterSet, (uint64_t)AlignedDataBufferBdlEntry, SoundDataSize / BdlEntriesRequired, BdlEntriesRequiredCurrentEntry, BdlEntriesRequiredCurrentEntry + 1);
    }

    // Write cyclic buffer length
    WriteValue = SoundDataSize * 2;
    WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_OSSN_OFFSET(0, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXCBL, (void *)&WriteValue, 1, 0, EfiPciWidthUint32);
    
    // Setup stream ID on codec nodes
    WriteValue8 = 0x10;
    GetCodecData8BitPayloadCorbRirb(PcieDeviceConfigSpace, 0, 0x10, HDA_VRB_SET_CHANNEL_STREAM_ID, WriteValue8, &Response);

    // Set stream ID
    WriteValue = 0x100000;
    WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_OSSN_OFFSET(0, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXCTL, (void *)&WriteValue, 1, 0, EfiPciWidthUint32);

    // Set FIFO size
    WriteValue = 0x04;
    WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_OSSN_OFFSET(0, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXFIFOS, (void *)&WriteValue, 1, 0, EfiPciWidthUint32);

    // Set stream format (2 channel, 16 bits, 44.1kHz)
    WriteValue16 = 0x4011;
    GetCodecData16BitPayloadCorbRirb(PcieDeviceConfigSpace, 0x0, 0x11, HDA_VRB_SET_STREAM_FORMAT, WriteValue16, &Response);
    WriteControllerRegister(PcieDeviceConfigSpace, CALCULATE_OSSN_OFFSET(0, ControllerRegisterSet->GCAP) + HDA_RELATIVE_OFFSET_SDXFMT, (void *)&WriteValue16, 1, 0, EfiPciWidthUint16);
    
    return EFI_SUCCESS;
}

EFI_STATUS uefi_entry(EFI_HANDLE ImageHandle,
                       EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;

    gImageHandle = ImageHandle;
    gSystemTable = SystemTable;
    gBootServices = SystemTable->BootServices;

    // clear the screen
    gSystemTable->ConOut->ClearScreen(gSystemTable->ConOut);

    // disable UEFI watchdog
    Status = gSystemTable->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
    if (EFI_ERROR(Status)) {
        debug("Couldn't disable UEFI watchdog: %s (%x)\n", efi_status_to_str(Status), Status);
    }

    // load Intel HDA driver
    HDA_CONTROLLER_REGISTER_SET ControllerRegisterSet;
    PCI_HDA_REGION PcieDeviceConfigSpace;
    uint32_t WriteValue = 0;
    uint16_t CurrentVolume = 12;

    SoundData = chime_data;
    SoundDataSize = chime_len;

    Status = InitHda();
    if (EFI_ERROR(Status)) {
        debug("uefi_entry(): Failed to initialize HDA driver!\n");
    } else {
        Status = GetPcieConfigSpace(HDA_BUS, HDA_DEV, HDA_FUNC, &PcieDeviceConfigSpace);
        if (EFI_ERROR(Status)) {
            debug("uefi_entry(): Failed to initialize HDA driver!\n");
        } else {
            GetControllerRegisterSet(&PcieDeviceConfigSpace, &ControllerRegisterSet);
            InitHdaControllerCodecAndBuffers(&PcieDeviceConfigSpace, &ControllerRegisterSet);
            AllocateResourcesBasedOnFile(&PcieDeviceConfigSpace, &ControllerRegisterSet, 0, 0);

            WriteValue = 0x100002;
            WriteControllerRegister(&PcieDeviceConfigSpace, CALCULATE_OSSN_OFFSET(0, ControllerRegisterSet.GCAP) + HDA_RELATIVE_OFFSET_SDXCTL, (void *)&WriteValue, 1, 0, EfiPciWidthUint32);
        }
    }

    axboot_init();
    UNREACHABLE();
}
