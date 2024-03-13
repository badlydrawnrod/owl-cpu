#include "owl-cpu/owl-cpu.hpp"

#include <iostream>

auto main() -> int
{
    ExportedClass myClass;
    std::cout << "Hello from " << myClass.name() << '\n';
}
