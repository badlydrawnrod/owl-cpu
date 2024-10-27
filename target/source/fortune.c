#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

int main()
{
    srand(time(NULL));
    size_t size = sizeof(aphorisms) / sizeof(char*);
    int i = rand() % size;
    puts(aphorisms[i]);
}
