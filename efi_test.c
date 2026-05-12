#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t CHAR16;
typedef uint32_t u32;
typedef uint64_t UINTN;
typedef uint64_t EFI_STATUS;
typedef void *EFI_HANDLE;

#define EFI_SUCCESS 0
#define EFIAPI __attribute__((ms_abi))

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_BOOT_SERVICES;

typedef EFI_STATUS (EFIAPI *efi_text_string_t)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
);

typedef EFI_STATUS (EFIAPI *efi_stall_t)(
    UINTN Microseconds
);

typedef void (EFIAPI *efi_copy_mem_t)(
    void *Destination,
    const void *Source,
    UINTN Length
);

typedef void (EFIAPI *efi_set_mem_t)(
    void *Buffer,
    UINTN Size,
    u8 Value
);

typedef EFI_STATUS (EFIAPI *efi_calculate_crc32_t)(
    const void *Data,
    UINTN DataSize,
    u32 *Crc32
);

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    efi_text_string_t OutputString;
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    void *ClearScreen;
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
};

struct EFI_TABLE_HEADER {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
};

struct EFI_BOOT_SERVICES {
    struct EFI_TABLE_HEADER Hdr;

    void *RaiseTPL;
    void *RestoreTPL;

    void *AllocatePages;
    void *FreePages;
    void *GetMemoryMap;
    void *AllocatePool;
    void *FreePool;

    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;

    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    void *HandleProtocol;
    void *Reserved;
    void *RegisterProtocolNotify;
    void *LocateHandle;
    void *LocateDevicePath;
    void *InstallConfigurationTable;

    void *LoadImage;
    void *StartImage;
    void *Exit;
    void *UnloadImage;
    void *ExitBootServices;

    void *GetNextMonotonicCount;
    efi_stall_t Stall;
    void *SetWatchdogTimer;

    void *ConnectController;
    void *DisconnectController;

    void *OpenProtocol;
    void *CloseProtocol;
    void *OpenProtocolInformation;

    void *ProtocolsPerHandle;
    void *LocateHandleBuffer;
    void *LocateProtocol;
    void *InstallMultipleProtocolInterfaces;
    void *UninstallMultipleProtocolInterfaces;

    efi_calculate_crc32_t CalculateCrc32;

    efi_copy_mem_t CopyMem;
    efi_set_mem_t SetMem;
    void *CreateEventEx;
};

struct EFI_SYSTEM_TABLE {
    struct EFI_TABLE_HEADER Hdr;

    CHAR16 *FirmwareVendor;
    uint32_t FirmwareRevision;

    EFI_HANDLE ConsoleInHandle;
    void *ConIn;

    EFI_HANDLE ConsoleOutHandle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;

    EFI_HANDLE StandardErrorHandle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;

    void *RuntimeServices;
    struct EFI_BOOT_SERVICES *BootServices;

    uint64_t NumberOfTableEntries;
    void *ConfigurationTable;
};

static CHAR16 msg_start[] = {
    '\r','\n',
    '=','=','=','=','=','=','=','=','=','=','=','=','=','=','=','=','=','=','=','=',
    '\r','\n',
    'B','o','o','t','S','e','r','v','i','c','e','s',' ','m','e','m',' ','t','e','s','t',
    '\r','\n',
    '=','=','=','=','=','=','=','=','=','=','=','=','=','=','=','=','=','=','=','=',
    '\r','\n',
    0
};

static CHAR16 msg_ok[] = {
    'S','e','t','M','e','m','/','C','o','p','y','M','e','m','/','C','R','C','3','2',
    ' ','w','o','r','k','e','d','.',
    '\r','\n',
    0
};

static CHAR16 msg_fail[] = {
    'B','o','o','t','S','e','r','v','i','c','e','s',' ','m','e','m',' ','t','e','s','t',
    ' ','f','a','i','l','e','d','.',
    '\r','\n',
    0
};

static CHAR16 source_msg[] = {
    'H','e','l','l','o',' ','f','r','o','m',' ','C','o','p','y','M','e','m','!',
    '\r','\n',
    0
};

static CHAR16 copied_msg[64];

static int str_eq16(const CHAR16 *a, const CHAR16 *b)
{
    while (*a || *b) {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }

    return 1;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable)
{
    (void)ImageHandle;

    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    struct EFI_BOOT_SERVICES *BS;
    u32 crc = 0;

    static CHAR16 step1[] = { 'b','e','f','o','r','e',' ','S','e','t','M','e','m','\r','\n',0 };
    static CHAR16 step2[] = { 'a','f','t','e','r',' ','S','e','t','M','e','m','\r','\n',0 };
    static CHAR16 step3[] = { 'a','f','t','e','r',' ','C','o','p','y','M','e','m','\r','\n',0 };
    static CHAR16 step4[] = { 'a','f','t','e','r',' ','s','t','r','_','e','q','1','6','\r','\n',0 };
    static CHAR16 step5[] = { 'a','f','t','e','r',' ','C','R','C','3','2','\r','\n',0 };
    static CHAR16 step6[] = { 'a','f','t','e','r',' ','S','t','a','l','l','\r','\n',0 };

    if (!SystemTable)
        return 1;

    ConOut = SystemTable->ConOut;
    BS = SystemTable->BootServices;

    if (!ConOut || !ConOut->OutputString)
        return 2;

    if (!BS)
        return 3;

    if (!BS->Stall || !BS->SetMem || !BS->CopyMem || !BS->CalculateCrc32)
        return 4;

    ConOut->OutputString(ConOut, msg_start);

    ConOut->OutputString(ConOut, step1);
    BS->SetMem(copied_msg, sizeof(copied_msg), 0);

    ConOut->OutputString(ConOut, step2);
    BS->CopyMem(copied_msg, source_msg, sizeof(source_msg));

    ConOut->OutputString(ConOut, step3);
    if (!str_eq16(copied_msg, source_msg)) {
        ConOut->OutputString(ConOut, msg_fail);
        return 5;
    }

    ConOut->OutputString(ConOut, step4);
    BS->CalculateCrc32(copied_msg, sizeof(source_msg), &crc);

    ConOut->OutputString(ConOut, step5);
    if (crc == 0) {
        ConOut->OutputString(ConOut, msg_fail);
        return 6;
    }

    ConOut->OutputString(ConOut, copied_msg);

    BS->Stall(500000);

    ConOut->OutputString(ConOut, step6);
    ConOut->OutputString(ConOut, msg_ok);

    return EFI_SUCCESS;
}