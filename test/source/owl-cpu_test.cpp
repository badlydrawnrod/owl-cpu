#include <string>

#include "owl-cpu/owl-cpu.hpp"

auto main() -> int
{
  auto const exported = ExportedClass {};

  return std::string("owl-cpu") == exported.name() ? 0 : 1;
}
