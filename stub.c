#include <stdint.h>
#include "pe_loader.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/*
 * PS4 Aeolia/Belize UART.
 *
 * Your Linux boot log used:
 *   console=uart8250,mmio32,0xd0340000
 */
#define UART_BASE 0xd0340000UL

/*
 * NOTE:
 * If UART output is very slow, try the mmio32 register layout instead:
 *
 *   #define UART_REG(reg) ((volatile u32 *)(UART_BASE + ((reg) * 4)))
 *   #define UART0_DATA UART_REG(0)
 *   #define UART0_LSR  UART_REG(5)
 *
 * For now this keeps your working byte-offset version.
 */
#define UART_REG(reg) ((volatile u32 *)(UART_BASE + ((reg) * 4)))
#define UART0_DATA UART_REG(0)
#define UART0_LSR  UART_REG(5)

#define LSR_TXRDY 0x20
#define LSR_TEMT  0x40

#define STUB_SCAN_START 0x06000000UL
#define STUB_SCAN_SIZE  0x00400000UL

static inline void compiler_barrier(void)
{
    __asm__ volatile ("" ::: "memory");
}

void uart_flush(void)
{
    int limit = 250000;

    while (!(*UART0_LSR & LSR_TEMT) && --limit)
        ;
}

void uart_write_byte(u8 b)
{
    int limit = 250000;

    while (!(*UART0_LSR & LSR_TXRDY) && --limit)
        ;

    *UART0_DATA = b;

    compiler_barrier();
}

void uart_write_char(char c)
{
    if (c == '\n')
        uart_write_byte('\r');

    uart_write_byte((u8)c);
}

void uart_write_str(const char *s)
{
    while (*s)
        uart_write_char(*s++);
}

void uart_write_hex_nibble(u8 v)
{
    v &= 0xf;

    if (v < 10)
        uart_write_char('0' + v);
    else
        uart_write_char('a' + (v - 10));
}

void uart_write_hex8(u8 value)
{
    uart_write_str("0x");
    uart_write_hex_nibble(value >> 4);
    uart_write_hex_nibble(value);
}

void uart_write_hex16(u16 value)
{
    uart_write_str("0x");

    for (int i = 12; i >= 0; i -= 4)
        uart_write_hex_nibble((u8)(value >> i));
}

void uart_write_hex32(u32 value)
{
    uart_write_str("0x");

    for (int i = 28; i >= 0; i -= 4)
        uart_write_hex_nibble((u8)(value >> i));
}

void uart_write_hex64(u64 value)
{
    uart_write_str("0x");

    for (int i = 60; i >= 0; i -= 4)
        uart_write_hex_nibble((u8)(value >> i));
}


// EFI HELPER

static EFI_STATUS EFIAPI fake_output_string(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
)
{
    (void)This;

    if (!String)
        return EFI_SUCCESS;

    while (*String) {
        CHAR16 ch = *String++;

        /*
         * For now, only handle ASCII-range CHAR16.
         * UEFI strings are UTF-16.
         */
        if (ch == L'\n') {
            uart_write_char('\n');
        } else if (ch < 0x80) {
            uart_write_char((char)ch);
        } else {
            uart_write_char('?');
        }
    }

    return EFI_SUCCESS;
}

static struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL fake_con_out = {
    .Reset = 0,
    .OutputString = fake_output_string,
    .TestString = 0,
    .QueryMode = 0,
    .SetMode = 0,
    .SetAttribute = 0,
    .ClearScreen = 0,
    .SetCursorPosition = 0,
    .EnableCursor = 0,
    .Mode = 0,
};

static EFI_STATUS EFIAPI fake_stall(UINTN Microseconds)
{
    /*
     * Crude busy wait.
     *
     * This is not accurate yet. It only gives EFI apps a callable Stall()
     * that does not crash.
     *
     * Tune STALL_LOOP_PER_US later if needed.
     */
    volatile u64 loops;
    u64 total;

    const u64 STALL_LOOP_PER_US = 50;

    total = Microseconds * STALL_LOOP_PER_US;

    for (loops = 0; loops < total; loops++) {
        __asm__ volatile ("pause");
    }

    return EFI_SUCCESS;
}

#define EFI_INVALID_PARAMETER 2

static EFI_STATUS EFIAPI fake_calculate_crc32(
    const void *Data,
    UINTN DataSize,
    u32 *Crc32
)
{
    const u8 *p = (const u8 *)Data;
    u32 crc = 0xFFFFFFFFU;

    if (!Data || !Crc32)
        return EFI_INVALID_PARAMETER;

    for (UINTN i = 0; i < DataSize; i++) {
        crc ^= p[i];

        for (int bit = 0; bit < 8; bit++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320U;
            else
                crc >>= 1;
        }
    }

    *Crc32 = ~crc;
    return EFI_SUCCESS;
}

static void EFIAPI fake_copy_mem(
    void *Destination,
    const void *Source,
    UINTN Length
)
{
    u8 *dst = (u8 *)Destination;
    const u8 *src = (const u8 *)Source;

    if (!Destination || !Source)
        return;

    if (dst < src) {
        for (UINTN i = 0; i < Length; i++)
            dst[i] = src[i];
    } else if (dst > src) {
        for (UINTN i = Length; i > 0; i--)
            dst[i - 1] = src[i - 1];
    }
}

static void EFIAPI fake_set_mem(void *Buffer, UINTN Size, u8 Value)
{
    uart_write_str("fake_set_mem entered\n");

    uart_write_str("Buffer: ");
    uart_write_hex64((u64)Buffer);
    uart_write_char('\n');

    uart_write_str("Size: ");
    uart_write_hex64((u64)Size);
    uart_write_char('\n');

    uart_write_str("Value: ");
    uart_write_hex8(Value);
    uart_write_char('\n');

    if (!Buffer) {
        uart_write_str("fake_set_mem NULL buffer\n");
        return;
    }

    u8 *p = (u8 *)Buffer;
    for (UINTN i = 0; i < Size; i++)
        p[i] = Value;

    uart_write_str("fake_set_mem leaving\n");
}

static CHAR16 fake_firmware_vendor[] = {
    'P', 'S', '4', '-', 'S', 't', 'u', 'b', 0
};

static struct EFI_BOOT_SERVICES fake_boot_services = {
    .Hdr = {
        .Signature = 0x56524553544f4f42ULL,
        .Revision = 0x00020000,
        .HeaderSize = sizeof(struct EFI_BOOT_SERVICES),
        .CRC32 = 0,
        .Reserved = 0,
    },

    .RaiseTPL = 0,
    .RestoreTPL = 0,

    .AllocatePages = 0,
    .FreePages = 0,
    .GetMemoryMap = 0,
    .AllocatePool = 0,
    .FreePool = 0,

    .CreateEvent = 0,
    .SetTimer = 0,
    .WaitForEvent = 0,
    .SignalEvent = 0,
    .CloseEvent = 0,
    .CheckEvent = 0,

    .InstallProtocolInterface = 0,
    .ReinstallProtocolInterface = 0,
    .UninstallProtocolInterface = 0,
    .HandleProtocol = 0,
    .Reserved = 0,
    .RegisterProtocolNotify = 0,
    .LocateHandle = 0,
    .LocateDevicePath = 0,
    .InstallConfigurationTable = 0,

    .LoadImage = 0,
    .StartImage = 0,
    .Exit = 0,
    .UnloadImage = 0,
    .ExitBootServices = 0,

    .GetNextMonotonicCount = 0,
    .Stall = fake_stall,
    .SetWatchdogTimer = 0,

    .ConnectController = 0,
    .DisconnectController = 0,

    .OpenProtocol = 0,
    .CloseProtocol = 0,
    .OpenProtocolInformation = 0,

    .ProtocolsPerHandle = 0,
    .LocateHandleBuffer = 0,
    .LocateProtocol = 0,
    .InstallMultipleProtocolInterfaces = 0,
    .UninstallMultipleProtocolInterfaces = 0,

    .CalculateCrc32 = fake_calculate_crc32,

    .CopyMem = fake_copy_mem,
    .SetMem = fake_set_mem,
    .CreateEventEx = 0,
};

static struct EFI_SYSTEM_TABLE fake_system_table = {
    .Hdr = {
        /*
         * EFI_SYSTEM_TABLE_SIGNATURE = "IBI SYST" little-endian.
         */
        .Signature = 0x5453595320494249ULL,
        .Revision = 0x00020000,
        .HeaderSize = sizeof(struct EFI_SYSTEM_TABLE),
        .CRC32 = 0,
        .Reserved = 0,
    },

    .FirmwareVendor = fake_firmware_vendor,
    .FirmwareRevision = 1,

    .ConsoleInHandle = 0,
    .ConIn = 0,

    .ConsoleOutHandle = (EFI_HANDLE)0x434f4e4f5554ULL,
    .ConOut = &fake_con_out,

    .StandardErrorHandle = (EFI_HANDLE)0x535444455252ULL,
    .StdErr = &fake_con_out,

    .RuntimeServices = 0,
    .BootServices = &fake_boot_services,

    .NumberOfTableEntries = 0,
    .ConfigurationTable = 0,
};

struct EFI_SYSTEM_TABLE *efi_get_system_table(void)
{
    return &fake_system_table;
}

static int magic_match(const u8 *p)
{
    return p[0] == 'E' &&
           p[1] == 'F' &&
           p[2] == 'I' &&
           p[3] == 'A' &&
           p[4] == 'P' &&
           p[5] == 'P' &&
           p[6] == '0' &&
           p[7] == '0';
}

static u64 read_le64(const u8 *p)
{
    return ((u64)p[0])       |
           ((u64)p[1] << 8)  |
           ((u64)p[2] << 16) |
           ((u64)p[3] << 24) |
           ((u64)p[4] << 32) |
           ((u64)p[5] << 40) |
           ((u64)p[6] << 48) |
           ((u64)p[7] << 56);
}

static void find_and_run_embedded_efi(void)
{
    u8 *start = (u8 *)STUB_SCAN_START;
    u8 *end   = start + STUB_SCAN_SIZE;

    uart_write_str("Searching for embedded EFI app...\n");

    for (u8 *p = start; p + 16 < end; p++) {
        if (magic_match(p)) {
            u64 size = read_le64(p + 8);
            void *efi = p + 16;

            uart_write_str("Found EFI app marker at: ");
            uart_write_hex64((u64)p);
            uart_write_char('\n');

            uart_write_str("EFI app payload at:      ");
            uart_write_hex64((u64)efi);
            uart_write_char('\n');

            uart_write_str("EFI app size:            ");
            uart_write_hex64(size);
            uart_write_char('\n');

            uart_write_str("Calling PE loader...\n");

            uart_write_str("stub fake_set_mem: ");
            uart_write_hex64((u64)fake_set_mem);
            uart_write_char('\n');

            uart_write_str("stub BS->SetMem: ");
            uart_write_hex64((u64)fake_boot_services.SetMem);
            uart_write_char('\n');

            uart_write_str("stub fake_copy_mem: ");
            uart_write_hex64((u64)fake_copy_mem);
            uart_write_char('\n');

            uart_write_str("stub BS->CopyMem: ");
            uart_write_hex64((u64)fake_boot_services.CopyMem);
            uart_write_char('\n');

            int rc = pe_load_and_start(efi, size);

            uart_write_str("PE loader returned: ");
            uart_write_hex32((u32)rc);
            uart_write_char('\n');

            return;
        }
    }

    uart_write_str("EFI app not found.\n");
}

void stub_main(void *boot_params)
{
    uart_write_str("\n");
    uart_write_str("========================================\n");
    uart_write_str(" HELLO FROM PS4 MINIMAL BOOT STUB\n");
    uart_write_str("========================================\n");

    uart_write_str("Stub reached 64-bit C code.\n");

    uart_write_str("boot_params pointer from RSI: ");
    uart_write_hex64((u64)boot_params);
    uart_write_char('\n');

    /*
     * Keep this disabled while testing PE loading.
     * Your old boot_params dump works, but it makes UART painfully noisy.
     */

    find_and_run_embedded_efi();

    uart_write_str("\nStub done. Spinning forever.\n");
    uart_flush();

    __asm__ volatile ("cli");

    for (;;) {
        __asm__ volatile ("pause");
    }
}