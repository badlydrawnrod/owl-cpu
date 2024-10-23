extern unsigned char _data_lma_start;
extern unsigned char _data_lma_end;
extern unsigned char _data_vma_start;

void init_vma(void)
{
    // Copy the .data section from LMA to VMA if they differ.
    unsigned char* dst = &_data_vma_start;
    unsigned char* src = &_data_lma_start;
    if (src != dst)
    {
        while (src < &_data_lma_end)
        {
            *dst++ = *src++;
        }
    }
}
