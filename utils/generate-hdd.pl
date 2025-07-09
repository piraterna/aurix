#!/usr/bin/env perl

use strict;
use File::Copy::Recursive qw(fcopy rcopy);
use File::Path qw(rmtree);
use Expect;

sub execute_command {
	my $cmd = $_[0] ;#. ' 2>/dev/null';
	my $output = `$cmd`;
	if ($? != 0) {
		die " failed: Command returned a non-zero value ($?).\n";
	}

	return $output;
}

# Check if the script was invoked by make
if (not defined $ENV{'AURIXBUILD'}) {
	die "Please don't invoke this script manually. Run \`make livecd\` instead.\n"
}

my $disk_name = $ARGV[0];
my $sysroot_size = 16; # Mb
my $efi_part_size = 32; # Mb
my $disk_size = $sysroot_size + $efi_part_size + 2;
my $sysroot = $ENV{'SYSROOT_DIR'};

if ($disk_name eq "") {
	die " failed: Target disk name not provided."
}

execute_command("dd if=/dev/zero of=" . $disk_name . " bs=1M count=" . $disk_size);

# fdisk
my $R = <STDIN>;
my $fdisk = Expect->spawn("fdisk $disk_name");

# GPT label
print $fdisk "g\n";

# Create a new primary partition of type 1
print $fdisk "n p\n";
print $fdisk "1\n";
print $fdisk "1\n";
print $fdisk "+$efi_part_size\M\n";
print $fdisk "t 1\n";
print $fdisk "1\n";
print $fdisk "n p\n";
print $fdisk "2\n";
print $fdisk "\n\n";
print $fdisk "t 2\n";
print $fdisk "11\n";
print $fdisk "w\n";
print $fdisk "q\n";

# mount, format

# copy
#fcopy($uefi_image, $tmp_mount_dir . "/boot/uefi.bin"); # or die ""
#fcopy($ENV{'BUILD_DIR'} . "/boot/pc-bios/stage1-cd.bin", $tmp_mount_dir . '/boot/bootcd.bin'); # or die ""
#rcopy($ENV{'ROOT_DIR'} . "/sysroot/", $tmp_mount_dir); # or die ""

# unmount

# install BIOS bootloader

print " done.\n";
exit