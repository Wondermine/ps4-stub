#!/usr/bin/env python3

import struct
from pathlib import Path

SETUP_SECTS = 4
PROTECTED_OFFSET = (SETUP_SECTS + 1) * 512
ENTRY_OFFSET_INSIDE_PROTECTED = 0x200

stub = Path("stub.bin").read_bytes()
efi = Path("efi_test.efi").read_bytes()

embedded = b"EFIAPP00" + struct.pack("<Q", len(efi)) + efi

protected_blob = (
    b"\x90" * ENTRY_OFFSET_INSIDE_PROTECTED +
    stub +
    embedded
)

img = bytearray(PROTECTED_OFFSET)

img[0x1F1] = SETUP_SECTS
img[0x1F4:0x1F8] = struct.pack("<I", (len(protected_blob) + 15) // 16)

img[0x200:0x202] = b"\xEB\x66"
img[0x202:0x206] = b"HdrS"
img[0x206:0x208] = struct.pack("<H", 0x0208)

img[0x210] = 0xff
img[0x211] = 0x01

img[0x214:0x218] = struct.pack("<I", 0x6000200)
img[0x218:0x21C] = struct.pack("<I", 0)
img[0x21C:0x220] = struct.pack("<I", 0)
img[0x224:0x226] = struct.pack("<H", 0xFE00)
img[0x228:0x22C] = struct.pack("<I", 0)
img[0x22C:0x230] = struct.pack("<I", 0xFFFFFFFF)
img[0x230:0x234] = struct.pack("<I", 0x200000)
img[0x234] = 0
img[0x238:0x23C] = struct.pack("<I", 4096)
img[0x23C:0x240] = struct.pack("<I", 0)

img += protected_blob

Path("bzImage.stub").write_bytes(img)

print("wrote bzImage.stub")
print(f"stub size:        0x{len(stub):x}")
print(f"efi size:         0x{len(efi):x}")
print(f"protected size:   0x{len(protected_blob):x}")
print(f"syssize:          0x{((len(protected_blob) + 15) // 16):x}")
print("copy base:        0x6000000")
print("stub entry:       0x6000200")