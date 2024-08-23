#include <stdint.h>

static inline void print_fib(uint32_t i, uint32_t result)
{
    // Define local register variables and associate them with specific RISC-V registers.
    // See: https://gcc.gnu.org/onlinedocs/gcc/Local-Register-Variables.html
    //
    register uint32_t a7 __asm__("a7") = 1;      // a7 = syscall number
    register uint32_t a0 __asm__("a0") = i;      // a0 = first argument to syscall
    register uint32_t a1 __asm__("a1") = result; // a1 = second argument to syscall

    // Invoke the `PrintFib` syscall.
    //
    // See: https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
    //      https://gcc.gnu.org/onlinedocs/gcc/Simple-Constraints.html
    //      https://gcc.gnu.org/onlinedocs/gcc/Modifiers.html
    // and: https://www.felixcloutier.com/documents/gcc-asm.html
    //
    __asm__ __volatile__("ecall /* Do a syscall */"  // assembler template: ecall (with a comment)
                         : "+r"(a0)                  // output operands: reads and writes a0
                         : "r"(a7), "r"(a0), "r"(a1) // input operands: reads a7, a0, a1
                         : "memory");                // clobbers: this instruction may modify memory
}

static uint32_t fib(uint32_t n)
{
    if (n < 2)
    {
        return n;
    }
    uint32_t previous = 0;
    uint32_t current = 1;
    for (; n > 1; --n)
    {
        uint32_t next = current + previous;
        previous = current;
        current = next;
    }
    return current;
}

int main(void)
{
    for (uint32_t i = 0; i < 48; i++)
    {
        uint32_t result = fib(i);
        print_fib(i, result);
    }
    return 0;
}
