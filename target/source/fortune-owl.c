#include "syscalls.h"

#include <stddef.h>
#include <stdint.h>

// Read only data.
// The strings, and the array itself, are in the `.rodata` section.
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
static int intInSData = 0x12345678;                  // This is in the `.sdata` section.
static int arrayInData[] = {0, 1, 2, 3, 4, 5, 6, 7}; // This is in the `.data` section.
static char x = 'x';                                 // This is in the `.sdata` section.

// Uninitialised data.
static int intVal;       // This is in the `.sbss` section.
static int dieRolls[10]; // This is in the `.bss` section.

static void display(int i)
{
    // The jump table for this switch statement is in the `.rodata` section.
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
    // Confirm that `.sdata` contains the expected values.
    if ((x != 'x') || (intInSData != 0x12345678))
    {
        puts("ERROR: Failed to initialise .sdata");
        return 1;
    }

    // Confirm that `.data` contains the expected values.
    for (int i = 0; i < sizeof(arrayInData) / sizeof(int); i++)
    {
        if (arrayInData[i] != i)
        {
            puts("ERROR: Failed to initialise .data");
            return 1;
        }
    }

    // Confirm that `.sbss` has been zeroed.
    if (intVal != 0)
    {
        puts("ERROR: failed to zero .sbss");
        return 1;
    }

    // Confirm that `.bss` has been zeroed.
    for (int i = 0; i < sizeof(dieRolls) / sizeof(dieRolls[0]); i++)
    {
        if (dieRolls[i] != 0)
        {
            puts("ERROR: failed to zero .bss");
            return 1;
        }
    }

    const size_t numAphorisms = sizeof(aphorisms) / sizeof(char*);

    // Confirm that the array and strings can be read from `.rodata`.
    for (int i = 0; i < numAphorisms; i++)
    {
        puts(aphorisms[i]);
    }
    puts("");

    // Confirm that the switch statement's jump table can be read from `.rodata`.
    randomize();
    for (int i = 0; i < numAphorisms; i++)
    {
        display(random(numAphorisms));
    }
    puts("");

    // TODO: Demonstrate that `.rodata` is actually read-only.

    // Write to `.bss`.
    const size_t numDieRolls = sizeof(dieRolls) / sizeof(dieRolls[0]);
    for (int i = 0; i < numDieRolls; i++)
    {
        dieRolls[i] = (i + 1) * 10; // Deliberately non-zero.
    }

    // Confirm that `.bss` contains the expected values.
    for (int i = 0; i < numDieRolls; i++)
    {
        if (dieRolls[i] != (i + 1) * 10)
        {
            puts("ERROR: failed to write to .bss");
            return 1;
        }
    }

    // Write to `.sbss`.
    uint32_t n = random(numDieRolls);
    intVal = dieRolls[n];

    // Confirm that `.sbss` contains the expected value.
    if (intVal != dieRolls[n])
    {
        puts("ERROR: failed to write to .sbss");
        return 1;
    }


    return 0;
}
