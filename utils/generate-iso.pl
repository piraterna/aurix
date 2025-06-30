#!/usr/bin/env perl

use File::Copy;

sub execute_command {
	$output = `$_[0]`;
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
my $sysroot = $ENV{'SYSROOT_DIR'};

if ($disk_name eq "") {
	die " failed: Target disk name not provided."
}

# Create a UEFI image
my $uefi_image = $ENV{'BUILD_DIR'} . '/uefi.img';
my $tmp_mount_dir = execute_command("mktemp -d 2>/dev/null");

execute_command("dd if=/dev/zero of=" . $uefi_image . " bs=1k count=1440");
execute_command("mformat -i " . $uefi_image . " -f 1440 ::");
execute_command("mcopy -i " . $uefi_image . " -s " . $sysroot . "/EFI ::");
execute_command("mcopy -i " . $uefi_image . " -s " . $sysroot . "/System ::");
execute_command("mcopy -i " . $uefi_image . " -s " . $sysroot . "/AxBoot ::");

my $iso_boot_dir = $tmp_mount_dir . "/boot";
mkdir($iso_boot_dir . "/boot", 0700);

copy($uefi_image, $iso_boot_dir . "/uefi.bin"); # or die ""
copy($ENV{'BUILD_DIR'} . "/boot/pc-bios/stage1-cd.bin", $iso_boot_dir . "/bootcd.bin"); # or die ""
copy($ENV{'ROOT_DIR'} . "/sysroot/*", $tmp_mount_dir); # or die ""

execute_command("xorriso -as mkisofs -b boot/bootcd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot boot/uefi.bin -efi-boot-part --efi-boot-image --protective-msdos-label" . $tempmountdir . " -o " . $disk_name);
rmdir($tmp_mount_dir);

print " done.\n";
exit