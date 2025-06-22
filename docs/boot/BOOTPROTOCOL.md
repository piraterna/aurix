# Aurix Boot Protocol (revision 0.2)

The Aurix Boot Protocol presents a simple and minimal protocol for booting the AurixOS kernel.

> [!NOTE]  
> This document is still a work in progress and may contain incomplete information.

## Machine state

- All general purpose registers are zeroed out
- Interrupts are disabled

- Framebuffer is set to the best available video mode (graphics mode if available)

### Architecture-specific

#### x86_64

- Write Protection bit in CR0 is disabled

- GDT is set up as follows:

| Name                   | Base   | Limit        | Flags      |
| :--------------------- | :----: | :----------: | :--------: |
| NULL Descriptor        | `0x00` | `0x0000`     | `0x00`     |
| 32-bit Code Descriptor | `0x00` | `0xFFFFFFFF` | Read only  |
| 32-bit Data Descriptor | `0x00` | `0xFFFFFFFF` | Read/Write |
| 64-bit Code Descriptor | `0x00` | `0x0000`     | Read only  |
| 64-bit Data Descriptor | `0x00` | `0x0000`     | Read/Write |

## Paging

- ~~If available, 5-level paging is set up (see [Kernel Parameters](#kernel-parameters))~~ 5-level paging is not yet supported in AxBoot
- The memory map is identity mapped
- Kernel is mapped to the higher half if desired

## Kernel parameters

The bootloader passes `aurix_parameters` structure as a parameter to the kernel.

```c
struct aurix_parameters {
	// PROTOCOL INFO
	uint8_t revision;

	// MEMORY
	struct aurix_memmap *mmap;
	uint32_t mmap_entries;

	uintptr_t kernel_addr; // physical address

	// RSDP and SMBIOS
	uintptr_t rsdp_addr;
	uintptr_t smbios_addr;

	// FRAMEBUFFER
	struct aurix_framebuffer *framebuffer;
};
```

## ACPI and SMBIOS

If found, `rsdp_addr` and `smbios_addr` will always point to a valid ACPI RSDP and SMBIOS structures respectively, otherwise the addresses will be set to NULL.

## Memory map

The memory map is an array of entries containing the physical address, its length and type. The amount of entries is specified in the `mmap_entries` variable.

Entries are **not** guaranteed to not overlap with each other. They are however guaranteed to be sorted by base address from low to high.

```c
enum aurix_memmap_entry_type {
	AURIX_MMAP_RESERVED = 0,
	
	AURIX_MMAP_ACPI_RECLAIMABLE = 1,
	AURIX_MMAP_ACPI_MAPPED_IO = 2,
	AURIX_MMAP_ACPI_MAPPED_IO_PORTSPACE = 3,
	AURIX_MMAP_ACPI_NVS = 4,

	AURIX_MMAP_BOOTLOADER_RECLAIMABLE = 6,
	AURIX_MMAP_USABLE = 7
};

struct aurix_memmap {
	uintptr_t base;
	uint32_t size;
	uint8_t type;
};
```

## Framebuffer

If a valid framebuffer was found, a pointer to a framebuffer structure is passed as a part of kernel parameters. If multiple framebuffers were found, AxBoot will pick the first (valid) one available.

```c
enum aurix_framebuffer_format {
	AURIX_FB_RGBA = 0,
	AURIX_FB_BGRA = 1
};

struct aurix_framebuffer {
	uintptr_t addr;
	uint32_t width;
	uint32_t height;
	uint8_t bpp;
	uint32_t pitch;
	int format;
};
```
