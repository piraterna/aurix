#!/bin/bash

if [[ -z $1 ]]; then
	printf "Please don't invoke this script manually. Run \`make release_hdd\` instead.\n"
	exit 1
fi

#sysroot_size=$(du -hsm kernel | cut -f 1)
sysroot_size=16
efi_partition_size=32

disk_name=$1
disk_size=$(($sysroot_size + $efi_partition_size + 2))

unamestr=$(uname)

# Check if we're running on a supported host
if ! [[ "$unamestr" =~ ^('Linux'|'Darwin') ]]; then
	printf " failed. (unsupported host)\n"
	exit 128
fi

# create a disk image
dd if=/dev/zero of="$disk_name" bs=1M count="$disk_size" >/dev/null 2>&1

# create a partition table
fdisk "$disk_name" >/dev/null 2>&1 << EOF
g
n p
1

+$efi_partition_size\M
t 1
1
n p
2


t 2
11
w
EOF

tempmountdir=$(mktemp -d 2>/dev/null)

# mount disk
if [ "$unamestr" = 'Linux' ]; then
	echo "Linux HDD Generation not implemented yet"
	rm -r $tempmountdir
	exit 200
elif [ "$unamestr" = 'Darwin' ]; then
	loopback=$(hdiutil attach -nomount $disk_name)
	loopback=$(echo $loopback | cut -f1 -d" ")

	loopback_efi="$loopback"s1
fi

# format EFI partition
mkfs.vfat -F32 -n "EFI" "$loopback_efi" >/dev/null 2>&1

# mount EFI partition
if [ "$unamestr" = 'Darwin' ]; then
	diskutil mount -mountPoint "$tempmountdir" "$loopback_efi" >/dev/null 2>&1
else
	mount "$loopback_efi" "$tempmountdir"
fi

# Copy system root to the newly created image
cp -r "$ROOT_DIR"/sysroot/* "$tempmountdir"

# unmount all partitions
if [ "$unamestr" = 'Linux' ]; then
	echo "Linux HDD Generation not implemented yet"
	rm -r $tempmountdir
	exit 200
elif [ "$unamestr" = 'Darwin' ]; then
	diskutil unmount "$tempmountdir" >/dev/null 2>&1
	diskutil unmountDisk "$loopback" >/dev/null 2>&1
	hdiutil detach "$loopback" >/dev/null 2>&1
fi

rm -r "$tempmountdir"

# Install legacy BIOS placeholder to the new image
dd if="$BUILD_DIR/boot/pc-bios/stage1-hdd.bin" of="$disk_name" conv=notrunc bs=446 count=1 >/dev/null 2>&1
dd if="$BUILD_DIR/boot/pc-bios/stage1-hdd.bin" of="$disk_name" conv=notrunc bs=1 count=2 skip=510 seek=510 >/dev/null 2>&1

sync

printf " done.\n"
