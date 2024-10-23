/*
A basic linker script.

Defines the following symbols:

    __etext             The load address of the initialised data sections (`.data` and `.sdata).
    __data_start__      The address of the start of the initialised data sections in RAM.
    __data_end__        The address of the end of the initialised data sections in RAM.
    __bss_start__       The address of the start of the uninitialised data sections (`.sbss` and `.bss`) in RAM.
    __bss_end__         The address of the end of the uninitialised data sections in RAM.
    __end__             The start of the heap in RAM.
    __heap_start__      The start of the heap in RAM.
    __stack             The address of the stack in RAM.
    __global_pointer$   The address of the global pointer in RAM.

*/

OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")

OUTPUT_ARCH(riscv)
ENTRY(_start)

/* Configuration variables to be used by the MEMORY command. */
MEM_START = 0x00000000;
MEM_SIZE = 4K;

/* Memory space definition. These blocks of memory will be used in the SECTIONS command. */
MEMORY
{
    RAM (rw) : ORIGIN = MEM_START, LENGTH = MEM_SIZE
}

/* TODO: region aliases (if that makes things simpler) */

SECTIONS
{
    /* Startup code. */
    .init : ALIGN(4)
    {
        KEEP (*(SORT_NONE(.init)))
    } > RAM /* TODO: use an alias */

    /* Code. */
    .text : ALIGN(4)
    {
        *(.text)
    } > RAM /* TODO: use an alias */

    /* Read-only data. */
    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    } > RAM /* TODO: use an alias */

    /* Initialised data. */
    .data : ALIGN(4)
    {
        __data_start__ = .;
        *(.data)
    } > RAM /* TODO: use an alias */

    /* Small initialised data. */
    .sdata : ALIGN(4)
    {
        __sdata_start__ = .;
        *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata .srodata.*)
        *(.sdata .sdata.* .gnu.linkonce.s.*)
    } > RAM /* TODO: use an alias */
    
    __data_end__ = .;

    __etext = LOADADDR(.data);

    /* Small uninitialised data. */
    .sbss (NOLOAD) : ALIGN(4)
    {
        __bss_start__ = .;
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
    } > RAM /* TODO: use an alias */

    /* Uninitialised data. */
    .bss (NOLOAD) : ALIGN(4)
    {
        *(.bss .bss.* .gnu.linkonce.b.*)
    } > RAM /* TODO: use an alias */

    __bss_end__ = .;

    /DISCARD/ :
    {
        *(.eh_frame)
    }

    . = ALIGN(4);

    /* Start of the heap. */
    __end__ = .;
    __heap_start__ = .;

    /* Find an optimal location for the global pointer. */    
    __global_pointer$ = MIN(__sdata_start__ + 0x800, MAX(__data_start__ + 0x800, __bss_end__ - 0x800));

    _end = .; PROVIDE (end = .);

    /* The stack pointer. */
    __stack = ORIGIN(RAM) + LENGTH(RAM);
}
