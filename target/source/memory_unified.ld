/* Memory definitions for an address space that's 4K of RAM. */
OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")

OUTPUT_ARCH(riscv)
ENTRY(_start)

/* Memory space definition. These blocks of memory will be used in the SECTIONS command. */
MEMORY
{
    RAM (rwx) : ORIGIN = 0, LENGTH = 4K
}

REGION_ALIAS("REGION_TEXT", RAM)
REGION_ALIAS("REGION_RODATA", RAM)
REGION_ALIAS("REGION_DATA", RAM)
REGION_ALIAS("REGION_BSS", RAM)
