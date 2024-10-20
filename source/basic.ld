OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")

OUTPUT_ARCH(riscv)
ENTRY(_start)

/* Configuration variables to be used by the MEMORY command. */
MEM_START = 0x00000000;
MEM_SIZE = 4K;

/* The stack grows down from the end of memory. */
STACK_TOP = MEM_START + MEM_SIZE;

/* Memory space definition. These blocks of memory will be used in the SECTIONS command. */
MEMORY
{
    mem (rw) :  ORIGIN = MEM_START, LENGTH = MEM_SIZE
}

SECTIONS
{
    PROVIDE (__stack_top = STACK_TOP);

    /* Startup code */
    .init MEM_START :
    {
        KEEP (*(SORT_NONE(.init)))
    } > mem

    /* Code. */
    .text : ALIGN(4)
    {
        *(.text)
    } > mem

    /* Read-only data. */
    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    } > mem

    /* Initialised data. */
    .data : ALIGN(4)
    {
        __DATA_BEGIN__ = .;
        *(.data)
    } > mem

    /* Small initialised data. */
    .sdata : ALIGN(4)
    {
        __SDATA_BEGIN__ = .;
        *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata .srodata.*)
        *(.sdata .sdata.* .gnu.linkonce.s.*)
    } > mem

    _edata = .; PROVIDE (edata = .);

    /* Start of uninitialised data. */
    __bss_start = .;
    .sbss :
    {
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
    } > mem

    .bss (NOLOAD) :
    {
        *(.bss .bss.* .gnu.linkonce.b.*)
    } > mem

    /DISCARD/ :
    {
        *(.eh_frame)
    }

    . = ALIGN(32 / 8);
    __BSS_END__ = .;

    /* This may not make much sense if we later separate into "ROM" and "RAM". */
    /* __global_pointer$ = MIN(__SDATA_BEGIN__ + 0x800, MAX(__DATA_BEGIN__ + 0x800, __BSS_END__ - 0x800)); */

    _end = .; PROVIDE (end = .);
}
