#include "pe_loader.h"

#define PE_LOAD_BASE 0x08000000UL

#define IMAGE_DOS_SIGNATURE 0x5A4D      /* MZ */
#define IMAGE_NT_SIGNATURE  0x00004550  /* PE\0\0 */

#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5

#define IMAGE_REL_BASED_ABSOLUTE 0
#define IMAGE_REL_BASED_DIR64    10

struct image_dos_header {
    u16 e_magic;
    u16 e_cblp;
    u16 e_cp;
    u16 e_crlc;
    u16 e_cparhdr;
    u16 e_minalloc;
    u16 e_maxalloc;
    u16 e_ss;
    u16 e_sp;
    u16 e_csum;
    u16 e_ip;
    u16 e_cs;
    u16 e_lfarlc;
    u16 e_ovno;
    u16 e_res[4];
    u16 e_oemid;
    u16 e_oeminfo;
    u16 e_res2[10];
    u32 e_lfanew;
} __attribute__((packed));

struct image_file_header {
    u16 Machine;
    u16 NumberOfSections;
    u32 TimeDateStamp;
    u32 PointerToSymbolTable;
    u32 NumberOfSymbols;
    u16 SizeOfOptionalHeader;
    u16 Characteristics;
} __attribute__((packed));

struct image_data_directory {
    u32 VirtualAddress;
    u32 Size;
} __attribute__((packed));

struct image_optional_header64 {
    u16 Magic;
    u8  MajorLinkerVersion;
    u8  MinorLinkerVersion;
    u32 SizeOfCode;
    u32 SizeOfInitializedData;
    u32 SizeOfUninitializedData;
    u32 AddressOfEntryPoint;
    u32 BaseOfCode;
    u64 ImageBase;
    u32 SectionAlignment;
    u32 FileAlignment;
    u16 MajorOperatingSystemVersion;
    u16 MinorOperatingSystemVersion;
    u16 MajorImageVersion;
    u16 MinorImageVersion;
    u16 MajorSubsystemVersion;
    u16 MinorSubsystemVersion;
    u32 Win32VersionValue;
    u32 SizeOfImage;
    u32 SizeOfHeaders;
    u32 CheckSum;
    u16 Subsystem;
    u16 DllCharacteristics;
    u64 SizeOfStackReserve;
    u64 SizeOfStackCommit;
    u64 SizeOfHeapReserve;
    u64 SizeOfHeapCommit;
    u32 LoaderFlags;
    u32 NumberOfRvaAndSizes;
    struct image_data_directory DataDirectory[16];
} __attribute__((packed));

struct image_nt_headers64 {
    u32 Signature;
    struct image_file_header FileHeader;
    struct image_optional_header64 OptionalHeader;
} __attribute__((packed));

struct image_section_header {
    u8  Name[8];
    u32 VirtualSize;
    u32 VirtualAddress;
    u32 SizeOfRawData;
    u32 PointerToRawData;
    u32 PointerToRelocations;
    u32 PointerToLinenumbers;
    u16 NumberOfRelocations;
    u16 NumberOfLinenumbers;
    u32 Characteristics;
} __attribute__((packed));

struct image_base_relocation {
    u32 VirtualAddress;
    u32 SizeOfBlock;
} __attribute__((packed));

extern void uart_write_str(const char *s);
extern void uart_write_char(char c);
extern void uart_write_hex64(u64 value);
extern void uart_write_hex32(u32 value);

static void *memcpy_simple(void *dst, const void *src, u64 n)
{
    u8 *d = dst;
    const u8 *s = src;

    while (n--)
        *d++ = *s++;

    return dst;
}

static void memset_simple(void *dst, u8 value, u64 n)
{
    u8 *d = dst;

    while (n--)
        *d++ = value;
}

static int check_range(u64 off, u64 size, u64 total)
{
    if (off > total)
        return 0;

    if (size > total - off)
        return 0;

    return 1;
}

static int apply_relocations(u8 *image, struct image_optional_header64 *opt, u64 loaded_base)
{
    struct image_data_directory *dir;
    u64 delta;

    dir = &opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

    if (!dir->VirtualAddress || !dir->Size) {
        uart_write_str("PE: no relocation directory\n");

        if (loaded_base != opt->ImageBase) {
            uart_write_str("PE: image not loaded at preferred base and no relocs\n");
            return -1;
        }

        return 0;
    }

    delta = loaded_base - opt->ImageBase;

    uart_write_str("PE: preferred ImageBase: ");
    uart_write_hex64(opt->ImageBase);
    uart_write_char('\n');

    uart_write_str("PE: actual load base:    ");
    uart_write_hex64(loaded_base);
    uart_write_char('\n');

    uart_write_str("PE: relocation delta:    ");
    uart_write_hex64(delta);
    uart_write_char('\n');

    if (delta == 0)
        return 0;

    u8 *reloc_ptr = image + dir->VirtualAddress;
    u8 *reloc_end = reloc_ptr + dir->Size;

    while (reloc_ptr < reloc_end) {
        struct image_base_relocation *block;
        u32 block_rva;
        u32 block_size;
        u32 count;
        u16 *entries;

        block = (struct image_base_relocation *)reloc_ptr;
        block_rva = block->VirtualAddress;
        block_size = block->SizeOfBlock;

        if (!block_size)
            break;

        count = (block_size - sizeof(*block)) / sizeof(u16);
        entries = (u16 *)(reloc_ptr + sizeof(*block));

        for (u32 i = 0; i < count; i++) {
            u16 entry = entries[i];
            u16 type = entry >> 12;
            u16 off  = entry & 0x0fff;

            if (type == IMAGE_REL_BASED_ABSOLUTE) {
                continue;
            } else if (type == IMAGE_REL_BASED_DIR64) {
                u64 *patch = (u64 *)(image + block_rva + off);
                *patch += delta;
            } else {
                uart_write_str("PE: unsupported relocation type: ");
                uart_write_hex32(type);
                uart_write_char('\n');
                return -1;
            }
        }

        reloc_ptr += block_size;
    }

    return 0;
}

int pe_load_and_start(void *pe_file, u64 pe_size)
{
    struct image_dos_header *dos;
    struct image_nt_headers64 *nt;
    struct image_optional_header64 *opt;
    struct image_section_header *sec;
    u8 *file = pe_file;
    u8 *image = (u8 *)PE_LOAD_BASE;
    efi_entry_t entry;

    uart_write_str("PE: loader entered\n");

    if (pe_size < sizeof(struct image_dos_header)) {
        uart_write_str("PE: file too small\n");
        return -1;
    }

    dos = (struct image_dos_header *)file;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        uart_write_str("PE: bad MZ signature\n");
        return -1;
    }

    if (!check_range(dos->e_lfanew, sizeof(struct image_nt_headers64), pe_size)) {
        uart_write_str("PE: bad e_lfanew\n");
        return -1;
    }

    nt = (struct image_nt_headers64 *)(file + dos->e_lfanew);

    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        uart_write_str("PE: bad PE signature\n");
        return -1;
    }

    opt = &nt->OptionalHeader;

    uart_write_str("PE: OptionalHeader.Magic: ");
    uart_write_hex32(opt->Magic);
    uart_write_char('\n');

    if (opt->Magic != 0x20b) {
        uart_write_str("PE: not PE32+\n");
        return -1;
    }

    uart_write_str("PE: sections: ");
    uart_write_hex32(nt->FileHeader.NumberOfSections);
    uart_write_char('\n');

    uart_write_str("PE: SizeOfImage: ");
    uart_write_hex32(opt->SizeOfImage);
    uart_write_char('\n');

    uart_write_str("PE: Entry RVA: ");
    uart_write_hex32(opt->AddressOfEntryPoint);
    uart_write_char('\n');

    memset_simple(image, 0, opt->SizeOfImage);

    if (!check_range(0, opt->SizeOfHeaders, pe_size)) {
        uart_write_str("PE: bad headers size\n");
        return -1;
    }

    memcpy_simple(image, file, opt->SizeOfHeaders);

    sec = (struct image_section_header *)((u8 *)&nt->OptionalHeader + nt->FileHeader.SizeOfOptionalHeader);

    for (u32 i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        u8 *dst;
        u8 *src;
        u32 copy_size;

        dst = image + sec[i].VirtualAddress;
        src = file + sec[i].PointerToRawData;

        copy_size = sec[i].SizeOfRawData;

        if (copy_size) {
            if (!check_range(sec[i].PointerToRawData, copy_size, pe_size)) {
                uart_write_str("PE: bad section raw range\n");
                return -1;
            }

            memcpy_simple(dst, src, copy_size);
        }
    }

    if (apply_relocations(image, opt, PE_LOAD_BASE) < 0) {
        uart_write_str("PE: relocation failed\n");
        return -1;
    }

    entry = (efi_entry_t)(image + opt->AddressOfEntryPoint);

    uart_write_str("PE: jumping to EFI entry: ");
    uart_write_hex64((u64)entry);
    uart_write_char('\n');

    /*
     * Fake ImageHandle and SystemTable for now.
     * Tiny test app ignores these.
     */
    struct EFI_SYSTEM_TABLE *st = efi_get_system_table();

    uart_write_str("PE: EFI_SYSTEM_TABLE: ");
    uart_write_hex64((u64)st);
    uart_write_char('\n');

    EFI_STATUS status = entry((EFI_HANDLE)0x12345678, st);

    uart_write_str("PE: EFI app returned status: ");
    uart_write_hex64(status);
    uart_write_char('\n');

    return 0;
}