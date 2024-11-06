/* Memory definitions for an address space that's split into 2K of ROM and 2K of RAM */
OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")

OUTPUT_ARCH(riscv)
ENTRY(_start)

/*
A basic linker script.

Defines the following symbols:

    __etext             The load address of the initialised data sections (`.data` and `.sdata).
    __data_lma_start__  Alias for __etext.
    __data_start__      The address of the start of the initialised data sections in RAM.
    __data_end__        The address of the end of the initialised data sections in RAM.
    __bss_start__       The address of the start of the uninitialised data sections (`.sbss` and `.bss`) in RAM.
    __bss_end__         The address of the end of the uninitialised data sections in RAM.
    __end__             The start of the heap in RAM.
    __heap_start__      Alias for __end__.
    __stack             The address of the stack in RAM.
    __global_pointer$   The address of the global pointer in RAM.

*/

SECTIONS
{
    /* Code. */
    .text : ALIGN(4)
    {
        *(.init)
        *(.text)
    }

    /* Read-only data. */
    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    /* Initialised data. */
    .data : ALIGN(4)
    {
        __data_start__ = .;
        *(.data)
    }

    /* Small initialised data. */
    .sdata : ALIGN(4)
    {
        __sdata_start__ = .;
        *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata .srodata.*)
        *(.sdata .sdata.* .gnu.linkonce.s.*)
    }
    
    . = ALIGN(4);
    __data_end__ = .;

    __etext = LOADADDR(.data);
    __data_lma_start__ = LOADADDR(.data);

    /* Small uninitialised data. */
    .sbss (NOLOAD) : ALIGN(4)
    {
        __bss_start__ = .;
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
    }

    /* Uninitialised data. */
    .bss (NOLOAD) : ALIGN(4)
    {
        *(.bss .bss.* .gnu.linkonce.b.*)
    }

    . = ALIGN(4);
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
    __stack = 0x8000;
}
