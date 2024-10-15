#include <stddef.h>
#include <stdint.h>

// Notes on syscall invocation and inline assembly.
//
// See: https://gcc.gnu.org/onlinedocs/gcc/Local-Register-Variables.html
//
// See: https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
//      https://gcc.gnu.org/onlinedocs/gcc/Simple-Constraints.html
//      https://gcc.gnu.org/onlinedocs/gcc/Modifiers.html
// and: https://www.felixcloutier.com/documents/gcc-asm.html
//

static inline void print_fib(uint32_t i, uint32_t result)
{
    // Define local register variables and associate them with specific RISC-V registers.
    register uint32_t a7 __asm__("a7") = 1;      // a7 = syscall number
    register uint32_t a0 __asm__("a0") = i;      // a0 = first argument to syscall
    register uint32_t a1 __asm__("a1") = result; // a1 = second argument to syscall

    // Invoke the `print_fib` syscall.
    __asm__ __volatile__(
            "ecall /* syscall(print_fib) */" // assembler template: ecall (with a comment)
            : "+r"(a0)                       // output operands: reads and writes a0
            : "r"(a7), "r"(a0), "r"(a1)      // input operands: reads a7, a0, a1
            : "memory");                     // clobbers: this instruction may modify memory
}

static inline void puts(const char* s)
{
    // Define local register variables and associate them with specific RISC-V registers.
    register uint32_t a7 __asm__("a7") = 2;              // a7 = syscall number
    register uint32_t a0 __asm__("a0") = (uintptr_t)(s); // a0 = first argument to syscall

    // Invoke the `puts` syscall.
    __asm__ __volatile__("ecall /* syscall(puts) */" // assembler template: ecall (with a comment)
                         : "+r"(a0)                  // output operands: reads and writes a0
                         : "r"(a7), "r"(a0)          // input operands: reads a7, a0
                         : "memory");                // clobbers: this instruction may modify memory
}

static inline void randomize(void)
{
    // Define local register variables and associate them with specific RISC-V registers.
    register uint32_t a7 __asm__("a7") = 3; // a7 = syscall number

    // Invoke the `randomize` syscall.
    __asm__ __volatile__(
            "ecall /* syscall(randomize) */" // assembler template: ecall (with a comment)
            :                                // output operands: none
            : "r"(a7)                        // input operands: reads a7
            : "memory");                     // clobbers: this instruction may modify memory
}

static inline uint32_t random(uint32_t n)
{
    // Define local register variables and associate them with specific RISC-V registers.
    register uint32_t a7 __asm__("a7") = 4; // a7 = syscall number
    register uint32_t a0 __asm__("a0") = n; // a0 = first argument to syscall

    // Invoke the `random` syscall.
    __asm__ __volatile__("ecall /* syscall(random) */" // assembler template: ecall (with a comment)
                         : "+r"(a0)                    // output operands: reads and writes a0
                         : "r"(a7), "r"(a0)            // input operands: reads a7, a0
                         : "memory"); // clobbers: this instruction may modify memory

    return a0;
}

static char* aphorisms[] = {
        "Absence makes the heart grow fonder.",
        "A chain is only as strong as its weakest link.",
        "Actions speak louder than words.",
        "Better late than never.",
        "Experience is the name everyone gives to their mistakes.",
        "If it ain't broke, don't fix it.",
        "If you lie down with dogs, you get up with fleas.",
        "Ignorance is bliss.",
        "Measure twice. Cut once.",
        "The road to hell is paved with good intentions.",
};

void display(int i)
{
    switch (i)
    {
    case 0:
        puts(aphorisms[9]);
        break;
    case 1:
        puts(aphorisms[8]);
        break;
    case 2:
        puts(aphorisms[7]);
        break;
    case 3:
        puts(aphorisms[6]);
        break;
    case 4:
        puts(aphorisms[5]);
        break;
    case 5:
        puts(aphorisms[4]);
        break;
    case 6:
        puts(aphorisms[3]);
        break;
    case 7:
        puts(aphorisms[2]);
        break;
    case 8:
        puts(aphorisms[1]);
        break;
    case 9:
        puts(aphorisms[0]);
        break;
    default:
        puts("Never trust your inputs.");
        break;
    }
}

int main()
{
    randomize();
    size_t size = sizeof(aphorisms) / sizeof(char*);

    uint32_t i = random(size);
    puts(aphorisms[i]);
    
    display(i);

    return 0;
}
