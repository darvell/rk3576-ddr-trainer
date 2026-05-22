ENTRY(_start)

MEMORY
{
    SRAM (rwx) : ORIGIN = 0x3ff81000, LENGTH = 96K
}

SECTIONS
{
    . = ORIGIN(SRAM);

    .text : ALIGN(16)
    {
        KEEP(*(.text.start))
        *(.text .text.*)
    } > SRAM

    .rodata : ALIGN(16)
    {
        *(.rodata .rodata.*)
    } > SRAM

    .data : ALIGN(16)
    {
        *(.data .data.*)
    } > SRAM

    .bss (NOLOAD) : ALIGN(16)
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    } > SRAM

    . = ALIGN(16);
    __image_end = .;
    __stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
}
