#!/bin/bash

if [[ -z $1 ]]; then
	printf "Please don't invoke this script manually. Run \`make livecd\` instead.\n"
	exit 1
fi

disk_name=$1

uefi_image=$BUILD_DIR/uefi.img
tempmountdir=$(mktemp -d 2>/dev/null)

# Create UEFI image
dd if=/dev/zero of=$uefi_image bs=1k count=1440 >/dev/null 2>&1
mformat -i $uefi_image -f 1440 :: >/dev/null 2>&1
## !FIXME: Huge hack! Make a filesystem.
mcopy -i $uefi_image -s $SYSROOT_DIR/EFI :: >/dev/null 2>&1
mcopy -i $uefi_image -s $SYSROOT_DIR/System :: >/dev/null 2>&1
mcopy -i $uefi_image -s $SYSROOT_DIR/AxBoot :: >/dev/null 2>&1

# Create directory structure
mkdir -p $tempmountdir/boot

cp $uefi_image $tempmountdir/boot/uefi.bin
cp $BUILD_DIR/boot/pc-bios/stage1-cd.bin $tempmountdir/boot/bootcd.bin
cp -r $ROOT_DIR/sysroot/* $tempmountdir/

# Create ISO
xorriso -as mkisofs -b boot/bootcd.bin \
	-no-emul-boot -boot-load-size 4 -boot-info-table \
	--efi-boot boot/uefi.bin \
	-efi-boot-part --efi-boot-image --protective-msdos-label \
	$tempmountdir -o $1 >/dev/null 2>&1

rm -rf $tempmountdir

printf " done.\n"