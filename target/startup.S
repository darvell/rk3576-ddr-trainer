.section .text.start,"ax"
.global _start
.type _start, %function
_start:
    mov x19, x0
    ldr x16, =__stack_top
    mov sp, x16

    ldr x0, =__bss_start
    ldr x1, =__bss_end
    mov x2, xzr
1:
    cmp x0, x1
    b.hs 2f
    str x2, [x0], #8
    b 1b

2:
    mov x0, x19
    bl rk3576_ddr_entry

3:
    wfe
    b 3b
.size _start, . - _start
