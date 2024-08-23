// Credit to: https://github.com/lluixhi/musl-riscv/blob/master/arch/riscv32/syscall_arch.h

#include <stdint.h>

#define __asm_syscall(...)                                                                         \
    __asm__ __volatile__("ecall\n\t" : "+r"(a0) : __VA_ARGS__ : "memory");                         \
    return a0;

static inline uint32_t print_fib(uint32_t i, uint32_t result)
{
    register uint32_t a7 __asm__("a7") = 1;
    register uint32_t a0 __asm__("a0") = i;
    register uint32_t a0 __asm__("a1") = result;
    __asm__("ecall\n");
    return a0;
}

// #define __asm_syscall(...)                                                                                                         \
//     asm volatile("ecall\n\t" : "+r"(a0) : __VA_ARGS__ : "memory");                                                                 \
//     return a0;

// static inline uint32_t syscall0(uint32_t n)
// {
//     register uint32_t a7 asm("a7") = n;
//     register uint32_t a0 asm("a0");
//     __asm_syscall("r"(a7))
// }

// static inline uint32_t syscall1(uint32_t n, uint32_t a)
// {
//     register uint32_t a7 asm("a7") = n;
//     register uint32_t a0 asm("a0") = a;
//     __asm_syscall("r"(a7), "0"(a0))
// }

// static inline uint32_t syscall2(uint32_t n, uint32_t a, uint32_t b)
// {
//     register uint32_t a7 asm("a7") = n;
//     register uint32_t a0 asm("a0") = a;
//     register uint32_t a1 asm("a1") = b;
//     __asm_syscall("r"(a7), "0"(a0), "r"(a1))
// }

// static inline uint32_t syscall3(uint32_t n, uint32_t a, uint32_t b, uint32_t c)
// {
//     register uint32_t a7 asm("a7") = n;
//     register uint32_t a0 asm("a0") = a;
//     register uint32_t a1 asm("a1") = b;
//     register uint32_t a2 asm("a2") = c;
//     __asm_syscall("r"(a7), "0"(a0), "r"(a1), "r"(a2))
// }

// static inline void sys_exit(int32_t status)
// {
//     syscall1(SYSCALL_EXIT, status);
// }

// static inline void sys_yield(void)
// {
//     syscall0(SYSCALL_YIELD);
// }
