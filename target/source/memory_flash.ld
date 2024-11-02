/* Memory definitions for an address space that's split into 2K of ROM and 2K of RAM */
OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")

OUTPUT_ARCH(riscv)
ENTRY(_start)

/* Memory space definition. These blocks of memory will be used in the SECTIONS command. */
MEMORY
{
    FLASH (rx) : ORIGIN = 0, LENGTH = 16K
    RAM  (!rx) : ORIGIN = 16K, LENGTH = 16K
}

REGION_ALIAS("REGION_TEXT", FLASH)
REGION_ALIAS("REGION_RODATA", FLASH)
REGION_ALIAS("REGION_DATA", RAM)
REGION_ALIAS("REGION_BSS", RAM)
