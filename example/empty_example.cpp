#include "owl-cpu/owl-cpu.h"

#include <iostream>

auto main() -> int
{
    ExportedClass myClass;
    std::cout << "Hello from " << myClass.name() << '\n';
}
