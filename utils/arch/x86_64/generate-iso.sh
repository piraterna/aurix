#!/bin/bash

set -euo pipefail

if [[ -z $1 ]]; then
	printf "Please don't invoke this script manually. Run \`make livecd\` instead.\n"
	exit 1
fi

disk_name=$1
sysroot_dir=$2

uefi_image=$BUILD_DIR/uefi.img
tempmountdir=$(mktemp -d 2>/dev/null)

sysroot_bytes=$(du -sb "$SYSROOT_DIR/EFI" "$SYSROOT_DIR/System" "$SYSROOT_DIR/AxBoot" | awk '{sum += $1} END {print sum}')
# Add 16 MiB of slack for FAT metadata and growth, then round up to full MiB.
uefi_img_bytes=$((sysroot_bytes + 16 * 1024 * 1024))
if (( uefi_img_bytes < 64 * 1024 * 1024 )); then
	uefi_img_bytes=$((64 * 1024 * 1024))
fi
uefi_img_bytes=$((((uefi_img_bytes + 1024 * 1024 - 1) / (1024 * 1024)) * (1024 * 1024)))
uefi_img_sectors=$((uefi_img_bytes / 512))

# Create UEFI image
dd if=/dev/zero of=$uefi_image bs=1 count=0 seek=$uefi_img_bytes >/dev/null 2>&1
mformat -i $uefi_image -T $uefi_img_sectors :: >/dev/null 2>&1
## !FIXME: Huge hack! Make a filesystem.
mcopy -i $uefi_image -s $SYSROOT_DIR/EFI :: >/dev/null 2>&1
mcopy -i $uefi_image -s $SYSROOT_DIR/System :: >/dev/null 2>&1
mcopy -i $uefi_image -s $SYSROOT_DIR/AxBoot :: >/dev/null 2>&1

# Create directory structure
mkdir -p $tempmountdir/boot

cp $uefi_image $tempmountdir/boot/uefi.bin
cp $BUILD_DIR/boot/pc-bios/stage1-cd.bin $tempmountdir/boot/bootcd.bin
cp -r $sysroot_dir/* $tempmountdir/

# Create ISO
xorriso -as mkisofs -b boot/bootcd.bin \
	-no-emul-boot -boot-load-size 4 -boot-info-table \
	--efi-boot boot/uefi.bin \
	-efi-boot-part --efi-boot-image --protective-msdos-label \
	$tempmountdir -o $1 >/dev/null 2>&1

rm -rf $tempmountdir

printf " done.\n"
