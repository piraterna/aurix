#!/usr/bin/env perl

use File::Copy::Recursive qw(fcopy rcopy);
use File::Path qw(rmtree);

sub execute_command {
	$cmd = $_[0] . ' 2>/dev/null';
	$output = `$cmd`;
	if ($? != 0) {
		die " failed: Command returned a non-zero value ($?).\n";
	}

	return $output;
}

# Check if the script was invoked by make
if (not defined $ENV{'AURIXBUILD'}) {
	die "Please don't invoke this script manually. Run \`make livecd\` instead.\n"
}

# Check if we're generating an ISO for a supported architecture
if ($ENV{'ARCH'} equ "i686" or $ENV{'ARCH'} equ "x86_64") {
	print " skipped (unsupported architecture).";
	exit;
}

my $disk_name = $ARGV[0];
my $sysroot = $ENV{'SYSROOT_DIR'};

if ($disk_name eq "") {
	die " failed: Target disk name not provided."
}

# Create a UEFI image
my $uefi_image = $ENV{'BUILD_DIR'} . '/uefi.img';
my $tmp_mount_dir = execute_command("mktemp -d");
$tmp_mount_dir = substr $tmp_mount_dir, 0, -1;

mkdir($tmp_mount_dir . '/boot', 0700);

execute_command("dd if=/dev/zero of=" . $uefi_image . " bs=1k count=1440");
execute_command("mformat -i " . $uefi_image . " -f 1440 ::");
execute_command("mcopy -i " . $uefi_image . " -s " . $sysroot . "/EFI ::");
execute_command("mcopy -i " . $uefi_image . " -s " . $sysroot . "/System ::");
execute_command("mcopy -i " . $uefi_image . " -s " . $sysroot . "/AxBoot ::");

fcopy($uefi_image, $tmp_mount_dir . "/boot/uefi.bin"); # or die ""
fcopy($ENV{'BUILD_DIR'} . "/boot/pc-bios/stage1-cd.bin", $tmp_mount_dir . '/boot/bootcd.bin'); # or die ""
rcopy($ENV{'ROOT_DIR'} . "/sysroot/", $tmp_mount_dir); # or die ""

execute_command("xorriso -as mkisofs -b boot/bootcd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot boot/uefi.bin -efi-boot-part --efi-boot-image --protective-msdos-label " . $tmp_mount_dir . " -o " . $disk_name);
rmtree($tmp_mount_dir);

print " done.\n";
exit