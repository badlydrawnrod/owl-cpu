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

// Read only data. The strings, and the array itself, go into the `.rodata` section.
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

// Initialised data.
static int intInSData = 0x12345678;      // This is in the .`sdata` section.
static int arrayInData[] = {1, 2, 3, 4}; // This is in the `.data` section.

// Uninitialised data.
static int intVal;       // This is in the `.sbss` section.
static int dieRolls[10]; // This is in the `.bss` section.

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

    // Demonstrate `.rodata` (read-only data).
    uint32_t i = random(size);
    puts(aphorisms[i]);
    display(i);

    // TODO: Demonstrate `.data` and `.sdata` (initialised data).
    // The compiler can't optimize this away because it doesn't know what `random()` is going to
    // return.
    intInSData += random(size);
    for (int j = 0; j < sizeof(arrayInData) / sizeof(int); j++)
    {
        arrayInData[j] += intInSData;
        intInSData += random(arrayInData[j]);
    }
    intVal = arrayInData[random(sizeof(arrayInData) / sizeof(int))];

    // TODO: Demonstrate `.bss` and `.sbss` (uninitialised data).
    for (int j = 0; j < 10; j++)
    {
        dieRolls[j] = random(6);
    }
    puts(aphorisms[dieRolls[random(10)]]);

    return intInSData + intVal;
}

/*

If this doesn't motivate a linker script then I don't know what will.

$ clang -Os --target=riscv32 -march=rv32i -mabi=ilp32 -ffreestanding -ffunction-sections \
      -fdata-sections -nostdlib -nodefaultlibs .\source\crt0.s .\source\fortune-owl.c -fuse-ld=lld \
      "-Wl,--gc-sections" \
      "-Wl,--section-start=.init=0" \
      "-Wl,--section-start=.text=100" \
      "-Wl,--section-start=.rodata=400" \
      "-Wl,--section-start=.sdata=600" \
      "-Wl,--section-start=.data=700" \
      "-Wl,--section-start=.sbss=800" \
      "-Wl,--section-start=.bss=900"

$ llvm-objdump.exe -h a.out

a.out:  file format elf32-littleriscv

Sections:
Idx Name              Size     VMA      Type
  0                   00000000 00000000
  1 .init             0000001c 00000000 TEXT
  2 .text             00000238 00000100 TEXT
  3 .rodata           000001df 00000400 DATA
  4 .sdata            00000004 00000600 DATA
  5 .data             00000010 00000700 DATA
  6 .sbss             00000004 00000800 BSS
  7 .bss              00000028 00000900 BSS
  8 .eh_frame         0000002c 00001928 DATA
  9 .riscv.attributes 0000001c 00000000
 10 .comment          00000029 00000000
 11 .symtab           000001a0 00000000
 12 .shstrtab         0000006c 00000000
 13 .strtab           0000009b 00000000

Don't extract .sbss or .bss as they're NOLOAD sections, i.e., we don't want to copy them into the
image. However, we're currently at the mercy of the VM itself to zero them.

$ llvm-objcopy a.out -O binary a.bin -j .init -j .text -j .rodata -j .sdata -j .data

*/
