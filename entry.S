.intel_syntax noprefix

.global _start
.extern stub_main

.section .text
.code64

/*
 * The PS4 payload's linux_thunk.S jumps to copied_base + 0x200.
 *
 * The thunk passes:
 *   RSI = boot_params pointer
 *
 * We move RSI into RDI because RDI is the first argument in the
 * x86_64 System V ABI used by C.
 */

_start:
    cli

    /*
     * Temporary stack.
     *
     * This is low memory and should be identity mapped by the payload.
     * Later we can choose a stack based on the memory map.
     */
    mov rsp, 0x700000
    and rsp, -16

    /*
     * Pass boot_params to C:
     *   stub_main(void *boot_params)
     */
    mov rdi, rsi

    call stub_main

.hang:
    hlt
    jmp .hang