@ minimal GBA startup: header, stack, .data copy, .bss clear, call main
.section .text.start, "ax"
.arm
.global _start
_start:
    b   rom_start
    .fill 188, 1, 0              @ logo + header, patched by tools/gbafix.py
rom_start:
    mov r0, #0x12                @ IRQ mode stack
    msr cpsr_c, r0
    ldr sp, =0x03007FA0
    mov r0, #0x1F                @ system mode stack
    msr cpsr_c, r0
    ldr sp, =0x03007E00
    ldr r0, =__data_lma          @ copy .data from ROM to IWRAM
    ldr r1, =__data_start
    ldr r2, =__data_end
1:  cmp r1, r2
    ldrlo r3, [r0], #4
    strlo r3, [r1], #4
    blo 1b
    ldr r0, =__bss_start         @ zero .bss
    ldr r1, =__bss_end
    mov r2, #0
2:  cmp r0, r1
    strlo r2, [r0], #4
    blo 2b
    ldr r0, =main
    bx  r0
.pool
