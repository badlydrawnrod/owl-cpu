OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")

OUTPUT_ARCH(riscv)
ENTRY(_start)

/* Memory space definition. These blocks of memory will be used in the SECTIONS command. */
MEMORY
{
    FLASH(rx) : ORIGIN = 0x00000000, LENGTH = 4K
    RAM(!rx)  : ORIGIN = 0x20000000, LENGTH = 4K
}

SECTIONS
{
    PROVIDE (__stack_top = STACK_TOP);

    /* Startup code */
    .init : ALIGN(4)
    {
        KEEP (*(SORT_NONE(.init)))
    } > FLASH

    /* Code. */
    .text : ALIGN(4)
    {
        *(.text)
    } > FLASH

    PROVIDE (etext = .);

    /* Read-only data. */
    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    } > FLASH

    /* Initialised data. */
    .data : ALIGN(4)
    {
        __DATA_BEGIN__ = .;
        *(.data)
    } > RAM AT> FLASH

    /* Small initialised data. */
    .sdata : ALIGN(4)
    {
        __SDATA_BEGIN__ = .;
        *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata .srodata.*)
        *(.sdata .sdata.* .gnu.linkonce.s.*)
    } > RAM AT> FLASH

    _edata = .; PROVIDE (edata = .);

    _sidata = LOADADDR(.data);

    PROVIDE(_data_lma_start = LOADADDR(.data));

    PROVIDE(_data_lma_end = LOADADDR(.data) + SIZEOF(.data) + SIZEOF(.sdata));
    PROVIDE(_data_vma_start = ADDR(.data));

    /* Start of uninitialised data. */
    __bss_start = .;

    /* Small uninitialised data. */
    .sbss :
    {
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
    } > RAM

    /* Uninitialised data. */
    .bss (NOLOAD) :
    {
        *(.bss .bss.* .gnu.linkonce.b.*)
    } > RAM

    /DISCARD/ :
    {
        *(.eh_frame)
    }

    . = ALIGN(32 / 8);
    __BSS_END__ = .;

    __global_pointer$ = MIN(__SDATA_BEGIN__ + 0x800, MAX(__DATA_BEGIN__ + 0x800, __BSS_END__ - 0x800));

    _end = .; PROVIDE (end = .);
}
