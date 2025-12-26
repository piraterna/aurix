#!/usr/bin/env bash

output=$1

ramdisk_files=$SYSROOT_DIR/ramdisk

if [[ -z $1 ]]; then
	printf "Please don't invoke this script manually. Run \`make install\` instead.\n"
	exit 1
fi

tar -C $ramdisk_files -cf $output . 2>&1 >/dev/null

if [ $? -ne 0 ]; then
    printf " failed: tar failed\n"
    exit 1
fi

rm -rf $ramdisk_files
