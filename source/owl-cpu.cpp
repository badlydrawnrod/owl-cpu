#include "owl-cpu/owl-cpu.hpp"

#include <string>

ExportedClass::ExportedClass() : m_name{"owl-cpu"} {}

auto ExportedClass::name() const -> char const* { return m_name.c_str(); }
