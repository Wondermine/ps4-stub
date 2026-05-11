ENTRY(_start)

SECTIONS
{
    /*
     * The PS4 payload thunk copies the protected-mode payload to
     * 0x6000000 and jumps to 0x6000200.
     *
     * So link our actual entry point at 0x6000200.
     */
    . = 0x6000200;

    .text : ALIGN(16) {
        *(.text*)
    }

    .rodata : ALIGN(16) {
        *(.rodata*)
    }

    .data : ALIGN(16) {
        *(.data*)
    }

    .bss : ALIGN(16) {
        *(.bss*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}