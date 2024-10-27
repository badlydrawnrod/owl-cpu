#include "syscalls.h"

#include <stddef.h>
#include <stdint.h>

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
static int intInSData = 0x12345678;                  // This is in the `.sdata` section.
static int arrayInData[] = {0, 1, 2, 3, 4, 5, 6, 7}; // This is in the `.data` section.
static char x = 'x';                                 // This is in the `.sdata` section.

// Uninitialised data.
static int intVal;       // This is in the `.sbss` section.
static int dieRolls[10]; // This is in the `.bss` section.

static void display(int i)
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

    // Demonstrate `.data` and `.sdata` (initialised data).
    if (intInSData != 0x12345678)
    {
        puts("Failed to initialise sdata");
    }

    // The compiler can't optimize this away because it doesn't know what `random()` is going to
    // return.
    intInSData += random(size);
    for (int j = 0; j < sizeof(arrayInData) / sizeof(int); j++)
    {
        if (arrayInData[j] != j)
        {
            puts("Failed to initialise data");
        }
        arrayInData[j] += intInSData;
        intInSData += random(arrayInData[j]);
        x += random(10);
    }
    intVal = arrayInData[random(sizeof(arrayInData) / sizeof(int))];

    // TODO: Demonstrate `.bss` and `.sbss` (uninitialised data).
    for (int j = 0; j < 10; j++)
    {
        dieRolls[j] = random(6);
    }
    puts(aphorisms[dieRolls[random(10)]]);

    return intInSData + intVal + x;
}
