#!/usr/bin/env python3
import argparse
import binascii
import datetime
import enum
import io
import os
import struct
import sys
import uuid
from typing import BinaryIO, Optional, Tuple, List, Dict
import yaml

FV_MAGIC = 0x4856465F

class VariableState(enum.IntFlag):
    VAR_ADDED = 0x40
    VAR_HEADER_VALID_ONLY = 0x7F ^ 0xFF
    VAR_DELETED = 0xFD ^ 0xFF
    VAR_IN_DELETED_TRANSITION = 0xFE ^ 0xFF

class VariableFlags(enum.IntFlag):
    NON_VOLATILE = 0x1
    BOOTSERVICE_ACCESS = 0x2
    RUNTIME_ACCESS = 0x4
    HARDWARE_ERROR_RECORD = 0x8
    AUTHENTICATED_WRITE_ACCESS = 0x10
    TIME_BASED_AUTHENTICATED_WRITE_ACCESS = 0x20
    APPEND_WRITE = 0x40

class UUIDRegistry:
    _uuids: Dict[uuid.UUID, str] = {}
    _names: Dict[str, uuid.UUID] = {}

    @classmethod
    def register(cls, uuid_str: str, name: str) -> uuid.UUID:
        u = uuid.UUID(uuid_str)
        cls._uuids[u] = name
        cls._names[name] = u
        return u

    @classmethod
    def resolve(cls, u: uuid.UUID) -> str:
        return cls._uuids.get(u, str(u))

    @classmethod
    def lookup(cls, name: str) -> uuid.UUID:
        return cls._names.get(name, uuid.UUID(name))
    

# COMMON UEFI GUIDs
gEfiSystemNvDataFvGuid = UUIDRegistry.register("8d2bf1ff-9676-8b4c-a985-2747075b4f50", "gEfiSystemNvDataFvGuid")
gEfiAuthenticatedVariableGuid = UUIDRegistry.register("782cf3aa-7b94-9a43-a180-2e144ec37792", "gEfiAuthenticatedVariableGuid")
gEdkiiVarErrorFlagGuid = UUIDRegistry.register("e87fb304-aef6-0b48-bdd5-37d98c5e89aa", "gEdkiiVarErrorFlagGuid")
gEfiMemoryTypeInformationGuid = UUIDRegistry.register("9f04194c-3741-d34d-9c10-8b97a83ffdfa", "gEfiMemoryTypeInformationGuid")
gMtcVendorGuid = UUIDRegistry.register("114070eb-0214-d311-8e77-00a0c969723b", "gMtcVendorGuid")
gEfiGlobalVariableGuid = UUIDRegistry.register("61dfe48b-ca93-d211-aa0d-00e098032b8c", "gEfiGlobalVariableGuid")
gEfiIScsiInitiatorNameProtocolGuid = UUIDRegistry.register("45493259-44ec-0d4c-b1cd-9db139df070c", "gEfiIscsiInitiatorNameProtocolGuid")
gEfiIp4Config2ProtocolGuid = UUIDRegistry.register("d16e445b-0be3-aa4f-871a-3654eca36080", "gEfiIp4Config2ProtocolGuid")
gEfiImageSecurityDatabaseGuid = UUIDRegistry.register("cbb219d7-3a3d-9645-a3bc-dad00e67656f", "gEfiImageSecurityDatabaseGuid")
gEfiSecureBootEnableDisableGuid = UUIDRegistry.register("f0a30bc7-af08-4556-99c4-001009c93a44", "gEfiSecureBootEnableDisableGuid")
gEfiCustomModeEnableGuid = UUIDRegistry.register("0cec76c0-2870-9943-a072-71ee5c448b9f", "gEfiCustomModeEnableGuid")
gIScsiConfigGuid = UUIDRegistry.register("16d6474b-d6a8-5245-9d44-ccad2e0f4cf9", "gIScsiConfigGuid")
gEfiCertDbGuid = UUIDRegistry.register("6ee5bed9-dc75-d949-b4d7-b534210f637a", "gEfiCertDbGuid")
gMicrosoftVendorGuid = UUIDRegistry.register("bd9afa77-5903-324d-bd60-28f4e78f784b", "gMicrosoftVendorGuid")
gEfiVendorKeysNvGuid = UUIDRegistry.register("e0e47390-ec60-6e4b-9903-4c223c260f3c", "gEfiVendorKeysNvGuid")
mBmHardDriveBootVariableGuid = UUIDRegistry.register("e1e9b7fa-dd39-2b4f-8408-e20e906cb6de", "mBmHardDriveBootVariableGuid")

class UEFITime:
    def __init__(self, t: Optional[datetime.datetime] = None):
        if t:
            self.year = t.year
            self.month = t.month
            self.day = t.day
            self.hour = t.hour
            self.minute = t.minute
            self.second = t.second
            self.pad1 = 0
            self.nanosecond = t.microsecond * 1000
            self.timezone = 0
            self.daylight = 0
            self.pad2 = 0
        else:
            self.year = self.month = self.day = self.hour = self.minute = self.second = 0
            self.pad1 = self.nanosecond = self.timezone = self.daylight = self.pad2 = 0

    @classmethod
    def deserialize(cls, data: bytes) -> 'UEFITime':
        o = cls()
        (o.year, o.month, o.day, o.hour, o.minute, o.second,
         o.pad1, o.nanosecond, o.timezone, o.daylight, o.pad2) = struct.unpack("<HBBBBBBIhBB", data)
        return o

    def serialize(self) -> bytes:
        return struct.pack(
            "<HBBBBBBIhBB", self.year, self.month, self.day, self.hour,
            self.minute, self.second, self.pad1, self.nanosecond,
            self.timezone, self.daylight, self.pad2
        )

    @property
    def time(self) -> Optional[datetime.datetime]:
        if self.year == 0:
            return None
        tz = datetime.timezone.utc if self.timezone == 2047 else datetime.timezone(datetime.timedelta(minutes=self.timezone))
        return datetime.datetime(self.year, self.month, self.day, self.hour, self.minute, self.second, self.nanosecond // 1000, tz)

class FirmwareVolumeHeader:
    def __init__(self):
        self.vector: bytes = b""
        self.fsUUID: uuid.UUID = uuid.UUID(int=0)
        self.fvLen: int = 0
        self.magic: int = 0
        self.flags: int = 0
        self.hdrLen: int = 0
        self.checksum: int = 0
        self.extHdrOff: int = 0
        self.reserved: int = 0
        self.rev: int = 0
        self.blkInfo: List[Tuple[int, int]] = []

    @classmethod
    def deserialize(cls, f: BinaryIO) -> 'FirmwareVolumeHeader':
        o = cls()
        (o.vector, o.fsUUID, o.fvLen, o.magic, o.flags, o.hdrLen,
         o.checksum, o.extHdrOff, o.reserved, o.rev) = struct.unpack("<16s16sQIIHHHBB", f.read(56))
        o.fsUUID = uuid.UUID(bytes=o.fsUUID)
        if o.magic != FV_MAGIC:
            raise ValueError(f"Invalid magic: expected {FV_MAGIC:#x}, got {o.magic:#x}")
        if o.fsUUID != gEfiSystemNvDataFvGuid:
            raise ValueError(f"Unexpected UUID: {o.fsUUID}, expected {gEfiSystemNvDataFvGuid}")
        o.blkInfo = []
        while True:
            numBlk, blkLen = struct.unpack("<II", f.read(8))
            if numBlk == 0 and blkLen == 0:
                break
            o.blkInfo.append((numBlk, blkLen))
        return o

    def serialize(self) -> bytes:
        b = struct.pack(
            "<16s16sQIIHHHBB", self.vector, self.fsUUID.bytes, self.fvLen, self.magic,
            self.flags, self.hdrLen, self.checksum, self.extHdrOff, self.reserved, self.rev
        )
        for numBlk, blkLen in self.blkInfo:
            b += struct.pack("<II", numBlk, blkLen)
        b += struct.pack("<II", 0, 0)
        return b

    @classmethod
    def create(cls) -> 'FirmwareVolumeHeader':
        o = cls()
        o.vector = b"\x00" * 16
        o.fsUUID = gEfiSystemNvDataFvGuid
        o.fvLen = 528 * 1024
        o.magic = FV_MAGIC
        o.flags = 0x4FEFF
        o.hdrLen = 72
        o.checksum = 0xB8AF
        o.extHdrOff = 0
        o.reserved = 0
        o.rev = 2
        o.blkInfo = [(132, 4096)]
        return o

    def print(self) -> None:
        print(f"{'Firmware Volume Header':=^80}")
        print(f"{'UUID':<25}: {UUIDRegistry.resolve(self.fsUUID)}")
        print(f"{'FV Length':<25}: {self.fvLen:,} bytes ({self.fvLen / 1024:.1f} KiB)")
        print(f"{'Flags':<25}: {self.flags:#010x}")
        print(f"{'Header Length':<25}: {self.hdrLen:,} bytes")
        print(f"{'Checksum':<25}: {self.checksum:#06x}")
        print(f"{'Ext. Header Offset':<25}: {self.extHdrOff:#x}")
        print(f"{'Revision':<25}: {self.rev}")
        print("\nBlocks:")
        for numBlk, blkLen in self.blkInfo:
            print(f"  {numBlk:,} * {blkLen:,} byte blocks ({numBlk * blkLen / 1024:.1f} KiB total)")
        print()

class VariableStoreHeader:
    def __init__(self):
        self.hdrUUID: uuid.UUID = uuid.UUID(int=0)
        self.len: int = 0
        self.fmt: int = 0
        self.state: int = 0
        self.reserved1: int = 0
        self.reserved2: int = 0

    @classmethod
    def deserialize(cls, f: BinaryIO) -> 'VariableStoreHeader':
        o = cls()
        (o.hdrUUID, o.len, o.fmt, o.state, o.reserved1, o.reserved2) = struct.unpack("<16sIBBHI", f.read(28))
        o.hdrUUID = uuid.UUID(bytes=o.hdrUUID)
        if o.hdrUUID != gEfiAuthenticatedVariableGuid:
            raise ValueError(f"Unexpected UUID: {o.hdrUUID}, expected {gEfiAuthenticatedVariableGuid}")
        if o.fmt != 0x5A:
            raise ValueError(f"Invalid format: expected 0x5A, got {o.fmt:#x}")
        if o.state != 0xFE:
            raise ValueError(f"Invalid state: expected 0xFE, got {o.state:#x}")
        return o

    def serialize(self) -> bytes:
        return struct.pack(
            "<16sIBBHI", self.hdrUUID.bytes, self.len, self.fmt,
            self.state, self.reserved1, self.reserved2
        )

    @classmethod
    def create(cls) -> 'VariableStoreHeader':
        o = cls()
        o.hdrUUID = gEfiAuthenticatedVariableGuid
        o.len = 262072
        o.fmt = 0x5A
        o.state = 0xFE
        o.reserved1 = o.reserved2 = 0
        return o

    def print(self) -> None:
        print(f"{'Variable Store Header':=^80}")
        print(f"{'Length':<25}: {self.len:,} bytes ({self.len / 1024:.1f} KiB)")
        print(f"{'Format':<25}: {self.fmt:#04x}")
        print(f"{'State':<25}: {self.state:#04x}")
        print()

class AuthenticatedVariable:
    def __init__(self):
        self.magic: int = 0
        self.state: int = 0
        self.reserved1: int = 0
        self.flags: int = 0
        self.monotonicCount: int = 0
        self.timestamp: UEFITime = UEFITime()
        self.pubKeyIdx: int = 0
        self.nameLen: int = 0
        self.dataLen: int = 0
        self.vendorUUID: uuid.UUID = uuid.UUID(int=0)
        self.name: str = ""
        self.data: bytes = b""

    @classmethod
    def deserialize(cls, f: BinaryIO) -> Optional['AuthenticatedVariable']:
        o = cls()
        (o.magic, o.state, o.reserved1, o.flags, o.monotonicCount,
         ts, o.pubKeyIdx, o.nameLen, o.dataLen, o.vendorUUID) = struct.unpack("<HBBIQ16sIII16s", f.read(60))
        o.vendorUUID = uuid.UUID(bytes=o.vendorUUID)
        o.timestamp = UEFITime.deserialize(ts)
        if o.magic == 0xFFFF:
            return None
        if o.magic != 0x55AA:
            raise ValueError(f"Invalid magic: expected 0x55AA, got {o.magic:#x}")
        o.name = f.read(o.nameLen).decode("utf-16le").rstrip("\0")
        o.data = f.read(o.dataLen)
        if f.tell() % 4:
            f.read(4 - (f.tell() % 4))
        return o

    @classmethod
    def deserializeFromDocument(cls, vendorID: str, name: str, doc: Dict) -> 'AuthenticatedVariable':
        o = cls()
        o.magic = 0x55AA
        o.reserved1 = 0
        o.flags = doc.get("Flags", 0)
        if not doc.get("Volatile", False):
            o.flags |= VariableFlags.NON_VOLATILE
        if doc.get("Boot Access", False):
            o.flags |= VariableFlags.BOOTSERVICE_ACCESS
        if doc.get("Runtime Access", False):
            o.flags |= VariableFlags.RUNTIME_ACCESS
        if doc.get("Hardware Error Record", False):
            o.flags |= VariableFlags.HARDWARE_ERROR_RECORD
        if doc.get("Authenticated Write Access", False):
            o.flags |= VariableFlags.AUTHENTICATED_WRITE_ACCESS
        if doc.get("Time Based Authenticated Write Access", False):
            o.flags |= VariableFlags.TIME_BASED_AUTHENTICATED_WRITE_ACCESS
        if doc.get("Append Write", False):
            o.flags |= VariableFlags.APPEND_WRITE
        o.timestamp = UEFITime(doc.get("Timestamp", None))
        o.monotonicCount = doc.get("Monotonic Count", 0)
        o.pubKeyIdx = doc.get("Public Key Index", 0)
        o.name = name
        o.data = doc.get("Data", b"")
        if isinstance(o.data, str):
            o.data = o.data.encode("utf-8")
        o.dataLen = len(o.data)
        o.nameLen = len(name) * 2 + 2
        o.state = (VariableState.VAR_ADDED | VariableState.VAR_HEADER_VALID_ONLY) ^ 0xFF
        o.vendorUUID = UUIDRegistry.lookup(vendorID)
        return o

    def serialize(self) -> bytes:
        name = self.name.encode("utf-16le")
        assert self.nameLen == len(name) + 2
        assert self.dataLen == len(self.data)
        b = struct.pack(
            "<HBBIQ16sIII16s", self.magic, self.state, self.reserved1, self.flags,
            self.monotonicCount, self.timestamp.serialize(), self.pubKeyIdx,
            self.nameLen, self.dataLen, self.vendorUUID.bytes
        )
        b += name + b"\0\0"
        b += self.data
        if len(b) % 4:
            b += b"\xFF" * (4 - (len(b) % 4))
        return b

    @property
    def isDeleted(self) -> bool:
        return bool((self.state ^ 0xFF) & VariableState.VAR_DELETED)

    def print(self) -> None:
        state = self.state ^ 0xFF
        stext = []
        for flag in VariableState:
            if state & flag:
                stext.append(flag.name)
                state ^= flag
        if state:
            stext.append(f"0x{state:x}")
        stext = " | ".join(stext) if stext else "None"

        ftext = []
        flags = self.flags
        for flag in VariableFlags:
            if flags & flag:
                ftext.append(flag.name)
                flags ^= flag
        if flags:
            ftext.append(f"0x{flags:08x}")
        ftext = " ".join(ftext) if ftext else "None"

        print(f"{'Authenticated Variable':=^80}")
        print(f"{'Name':<25}: {self.name!r}")
        print(f"{'Vendor UUID':<25}: {UUIDRegistry.resolve(self.vendorUUID)}")
        print(f"{'Monotonic Count':<25}: {self.monotonicCount}")
        print(f"{'Public Key Index':<25}: {self.pubKeyIdx}")
        print(f"{'State':<25}: {stext}")
        print(f"{'Flags':<25}: {ftext}")
        if t := self.timestamp.time:
            print(f"{'Timestamp':<25}: {t}")
        print(f"{'Data Length':<25}: {self.dataLen:,} bytes")
        hexdump(io.BytesIO(self.data), elide=True)
        print()

def hexdump(f: BinaryIO, offset: int = 0, limit: Optional[int] = None, elide: bool = False, lba: Optional[Tuple[int, int]] = False) -> int:
    fl = f.seek(0, io.SEEK_END) if limit is None else limit
    f.seek(0)
    offset_chars = len(hex(fl + offset)[2:])
    consumed = 0
    prev_line = None
    eliding = False
    while limit is None or consumed < limit:
        lr = min(16, limit - consumed if limit is not None else 16)
        d = f.read(lr)
        if not d:
            break
        if elide and d == prev_line:
            if not eliding:
                eliding = True
                print(f"{'*':^80}")
        else:
            eliding = False
            dh = binascii.hexlify(d).decode("ascii").ljust(32, " ")
            asc = "".join(chr(x) if 0x20 <= x <= 0x7E else "." for x in d)
            asc = f"|{asc}|".ljust(18, " ")
            offs = f"{offset:08x}"
            pre = ""
            if lba:
                blk_data_size, blk_size = lba if isinstance(lba, tuple) else (4096, 4608)
                om = offset % blk_size
                oms = "D" if om < blk_data_size else "H"
                if om >= blk_data_size:
                    om -= blk_data_size
                pre = f"{offset // blk_size:>{offset_chars}} {oms}{om:04x}> "
            print(f"{pre}{offs}  {' '.join(dh[i:i+2] for i in range(0, 32, 2))}  {asc}")
        offset += lr
        consumed += lr
        prev_line = d
    return consumed

def cmd_dump(args: Dict[str, any]) -> int:
    input_file = args["input_file"]
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' does not exist.", file=sys.stderr)
        return 1
    try:
        with open(input_file, "rb") as f:
            fvh = FirmwareVolumeHeader.deserialize(f)
            fvh.print()
            vsh = VariableStoreHeader.deserialize(f)
            vsh.print()
            while True:
                av = AuthenticatedVariable.deserialize(f)
                if not av:
                    break
                if not av.isDeleted or args.get("deleted"):
                    av.print()
        return 0
    except Exception as e:
        print(f"Error processing file: {e}", file=sys.stderr)
        return 1

def cmd_export(args: Dict[str, any]) -> int:
    input_file = args["input_file"]
    output_file = args.get("output_file")
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' does not exist.", file=sys.stderr)
        return 1
    try:
        doc = {"Variables": {}}
        with open(input_file, "rb") as f:
            fvh = FirmwareVolumeHeader.deserialize(f)
            vsh = VariableStoreHeader.deserialize(f)
            while True:
                av = AuthenticatedVariable.deserialize(f)
                if not av:
                    break
                if av.isDeleted:
                    continue
                vendor_id = UUIDRegistry.resolve(av.vendorUUID)
                doc["Variables"].setdefault(vendor_id, {})
                var_data = {
                    "Data": av.data,
                    "Monotonic Count": av.monotonicCount,
                    "Public Key Index": av.pubKeyIdx,
                }
                if not (av.flags & VariableFlags.NON_VOLATILE):
                    var_data["Volatile"] = True
                if av.flags & VariableFlags.BOOTSERVICE_ACCESS:
                    var_data["Boot Access"] = True
                if av.flags & VariableFlags.RUNTIME_ACCESS:
                    var_data["Runtime Access"] = True
                if av.flags & VariableFlags.HARDWARE_ERROR_RECORD:
                    var_data["Hardware Error Record"] = True
                if av.flags & VariableFlags.AUTHENTICATED_WRITE_ACCESS:
                    var_data["Authenticated Write Access"] = True
                if av.flags & VariableFlags.TIME_BASED_AUTHENTICATED_WRITE_ACCESS:
                    var_data["Time Based Authenticated Write Access"] = True
                if av.flags & VariableFlags.APPEND_WRITE:
                    var_data["Append Write"] = True
                flags = av.flags & ~sum(VariableFlags)
                if flags:
                    var_data["Flags"] = flags
                if t := av.timestamp.time:
                    var_data["Timestamp"] = t
                doc["Variables"][vendor_id][av.name] = var_data
        yaml_str = yaml.dump(doc, sort_keys=False)
        if output_file:
            if os.path.exists(output_file) and not args.get("force"):
                print(f"Error: Output file '{output_file}' exists. Use --force to overwrite.", file=sys.stderr)
                return 1
            with open(output_file, "w") as f:
                f.write(yaml_str)
        else:
            print(yaml_str)
        return 0
    except Exception as e:
        print(f"Error exporting file: {e}", file=sys.stderr)
        return 1

def cmd_compile(args: Dict[str, any]) -> int:
    input_file = args["input_file"]
    output_file = args["output_file"]
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' does not exist.", file=sys.stderr)
        return 1
    if os.path.exists(output_file) and not args.get("force"):
        print(f"Error: Output file '{output_file}' exists. Use --force to overwrite.", file=sys.stderr)
        return 1
    try:
        with open(input_file, "r") as f:
            doc = yaml.safe_load(f)
        if not isinstance(doc, dict) or "Variables" not in doc:
            print("Error: Invalid YAML structure. Expected 'Variables' key.", file=sys.stderr)
            return 1
        vs = []
        for vendor_id, vars_data in doc.get("Variables", {}).items():
            for name, var_data in vars_data.items():
                if not isinstance(var_data, dict):
                    print(f"Error: Invalid variable data for '{name}' in '{vendor_id}'.", file=sys.stderr)
                    return 1
                av = AuthenticatedVariable.deserializeFromDocument(vendor_id, name, var_data)
                vs.append(av)
        with open(output_file, "wb") as fo:
            fm = io.BytesIO(b"\xFF" * (528 * 1024))
            fm.write(FirmwareVolumeHeader.create().serialize())
            fm.write(VariableStoreHeader.create().serialize())
            for v in vs:
                fm.write(v.serialize())
                if fm.tell() % 4:
                    fm.write(b"\xFF" * (4 - (fm.tell() % 4)))
            if fm.tell() > 0x41000:
                raise ValueError("Too many variables to fit in file")
            fm.seek(0x41000)
            fm.write(binascii.unhexlify(b"2b29589e687c7d49a0ce6500fd9f1b952caf2c64feffffffe00f000000000000"))
            fm.seek(0)
            fo.write(fm.read())
        return 0
    except Exception as e:
        print(f"Error compiling file: {e}", file=sys.stderr)
        return 1

def cmd_generate_blank(args: Dict[str, any]) -> int:
    output_file = args["output_file"]
    if os.path.exists(output_file) and not args.get("force"):
        print(f"Error: Output file '{output_file}' exists. Use --force to overwrite.", file=sys.stderr)
        return 1
    try:
        with open(output_file, "wb") as fo:
            fm = io.BytesIO(b"\xFF" * (528 * 1024))
            fm.write(FirmwareVolumeHeader.create().serialize())
            fm.write(VariableStoreHeader.create().serialize())
            fm.seek(0x41000)
            fm.write(binascii.unhexlify(b"2b29589e687c7d49a0ce6500fd9f1b952caf2c64feffffffe00f000000000000"))
            fm.seek(0)
            fo.write(fm.read())
        return 0
    except Exception as e:
        print(f"Error generating blank file: {e}", file=sys.stderr)
        return 1

def main():
    parser = argparse.ArgumentParser(description="UEFI OVMF variable store manipulation tool")
    subparsers = parser.add_subparsers(dest="command", help="Subcommands")
    dump_parser = subparsers.add_parser("dump", help="Dump OVMF_VARS.fd in human-readable form")
    dump_parser.add_argument("input_file", help="OVMF_VARS.fd file to dump")
    dump_parser.add_argument("--deleted", "-d", action="store_true", help="Show deleted variables")
    dump_parser.add_argument("--verbose", "-v", action="store_true", help="Enable verbose output")
    export_parser = subparsers.add_parser("export", help="Export OVMF_VARS.fd as YAML")
    export_parser.add_argument("input_file", help="OVMF_VARS.fd file to export")
    export_parser.add_argument("--output-file", "-o", help="Output YAML file (default: stdout)")
    export_parser.add_argument("--force", "-f", action="store_true", help="Overwrite existing output file")
    compile_parser = subparsers.add_parser("compile", help="Generate OVMF_VARS.fd from YAML")
    compile_parser.add_argument("input_file", help="YAML file to compile")
    compile_parser.add_argument("output_file", help="Output OVMF_VARS.fd file")
    compile_parser.add_argument("--force", "-f", action="store_true", help="Overwrite existing output file")
    blank_parser = subparsers.add_parser("generate-blank", help="Generate an empty OVMF_VARS.fd")
    blank_parser.add_argument("output_file", help="Output OVMF_VARS.fd file")
    blank_parser.add_argument("--force", "-f", action="store_true", help="Overwrite existing output file")
    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        sys.exit(1)
    commands = {
        "dump": cmd_dump,
        "export": cmd_export,
        "compile": cmd_compile,
        "generate-blank": cmd_generate_blank
    }
    sys.exit(commands[args.command](vars(args)))

if __name__ == "__main__":
    main()