#include <stdint.h>


// We don't have an implementation of print_fib() yet.
extern void print_fib(uint32_t i, uint32_t result);

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
