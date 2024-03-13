#include "owl-cpu/owl-cpu.h"

#include <string>

auto main() -> int
{
    auto const exported = ExportedClass{};

    return std::string("owl-cpu") == exported.name() ? 0 : 1;
}
