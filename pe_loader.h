#pragma once

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u16 CHAR16;
typedef u64 UINTN;
typedef u64 EFI_STATUS;
typedef void *EFI_HANDLE;

#define EFI_SUCCESS 0

#define EFIAPI __attribute__((ms_abi))

struct EFI_SYSTEM_TABLE;
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_BOOT_SERVICES;

typedef EFI_STATUS (EFIAPI *efi_entry_t)(
    EFI_HANDLE image_handle,
    struct EFI_SYSTEM_TABLE *system_table
);

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

struct EFI_TABLE_HEADER {
    u64 Signature;
    u32 Revision;
    u32 HeaderSize;
    u32 CRC32;
    u32 Reserved;
};

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


/*
 * Partial EFI_BOOT_SERVICES layout.
 *
 * IMPORTANT:
 * We must preserve the official field order up to Stall.
 * Everything unsupported is void * for now.
 */
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
    u32 FirmwareRevision;

    EFI_HANDLE ConsoleInHandle;
    void *ConIn;

    EFI_HANDLE ConsoleOutHandle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;

    EFI_HANDLE StandardErrorHandle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;

    void *RuntimeServices;
    struct EFI_BOOT_SERVICES *BootServices;

    u64 NumberOfTableEntries;
    void *ConfigurationTable;
};

struct EFI_SYSTEM_TABLE *efi_get_system_table(void);

int pe_load_and_start(void *pe_file, u64 pe_size);