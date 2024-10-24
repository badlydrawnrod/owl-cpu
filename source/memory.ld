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
__MEM_START = 0x00000000;
__MEM_SIZE = 4K;

/* Memory space definition. These blocks of memory will be used in the SECTIONS command. */
MEMORY
{
    RAM (rw) : ORIGIN = __MEM_START, LENGTH = __MEM_SIZE
}
