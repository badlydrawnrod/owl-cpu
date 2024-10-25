extern unsigned char __data_start__;
extern unsigned char __data_end__;
extern unsigned char __data_lma_start__;

 __attribute__ ((section (".init")))
void init_vma(void)
{
    // Copy the .data section from LMA to VMA if they differ.
    unsigned char* dst = &__data_start__;
    unsigned char* src = &__data_lma_start__;
    if (src != dst)
    {
        while (dst < &__data_end__)
        {
            *dst++ = *src++;
        }
    }
}
