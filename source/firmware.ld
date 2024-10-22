OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")

OUTPUT_ARCH(riscv)
ENTRY(_start)

/* Configuration variables to be used by the MEMORY command. */
ROM_START = 0x00000000;
ROM_SIZE = 2K;
RAM_START = 0x20000000;
RAM_SIZE = 2K;

/* The stack grows down from the end of RAM. */
STACK_TOP = RAM_START + RAM_SIZE;

/* Memory space definition. These blocks of memory will be used in the SECTIONS command. */
MEMORY
{
    rom (rx)  : ORIGIN = ROM_START, LENGTH = ROM_SIZE
    ram (!rx) : ORIGIN = RAM_START, LENGTH = RAM_SIZE
}

SECTIONS
{
    PROVIDE (__stack_top = STACK_TOP);

    /* Startup code */
    .init ROM_START :
    {
        KEEP (*(SORT_NONE(.init)))
    } > rom

    /* Code. */
    .text : ALIGN(4)
    {
        *(.text)
    } > rom

    /* Read-only data. */
    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    } > rom

    /* Initialised data. */
    .data : ALIGN(4)
    {
        __DATA_BEGIN__ = .;
        *(.data)
    } > ram AT> rom

    /* Small initialised data. */
    .sdata : ALIGN(4)
    {
        __SDATA_BEGIN__ = .;
        *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata .srodata.*)
        *(.sdata .sdata.* .gnu.linkonce.s.*)
    } > ram AT> rom

    _edata = .; PROVIDE (edata = .);

    /* Start of uninitialised data. */
    __bss_start = .;

    /* Small uninitialised data. */
    .sbss :
    {
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
    } > ram

    /* Uninitialised data. */
    .bss (NOLOAD) :
    {
        *(.bss .bss.* .gnu.linkonce.b.*)
    } > ram

    /DISCARD/ :
    {
        *(.eh_frame)
    }

    . = ALIGN(32 / 8);
    __BSS_END__ = .;

    __global_pointer$ = MIN(__SDATA_BEGIN__ + 0x800, MAX(__DATA_BEGIN__ + 0x800, __BSS_END__ - 0x800));

    _end = .; PROVIDE (end = .);
}
