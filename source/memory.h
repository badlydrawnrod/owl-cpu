#pragma once

#include "endian.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <span>

using Memory = std::span<std::byte>;

constexpr uint32_t romStart = 0x0u;
constexpr uint32_t romSize = 0x4000;
constexpr uint32_t ramStart = 0x4000;
constexpr uint32_t ramSize = 0x4000;

template<uint32_t sz>
    requires(sz == 1) || (sz == 2) || (sz == 4)
inline bool IsReadable(uint32_t addr)
{

    return addr >= romStart && addr <= romStart + romSize + sz;
}

template<uint32_t sz>
    requires(sz == 1) || (sz == 2) || (sz == 4)
inline bool IsWritable(uint32_t addr)
{

    return addr >= ramStart && addr <= ramStart + ramSize + sz;
}

inline std::byte Read8(const Memory memory, uint32_t addr)
{
    if (!IsWritable<1>(addr) && !IsReadable<1>(addr))
    {
        throw std::runtime_error(std::format("Read8: Segmentation violation: 0x{:08x}", addr));
    }
    return memory[addr];
}

inline std::uint16_t Read16(const Memory memory, uint32_t addr)
{
    if (!IsWritable<2>(addr) && !IsReadable<2>(addr))
    {
        throw std::runtime_error(std::format("Read16: Segmentation violation: 0x{:08x}", addr));
    }

    // Owl-2820 is permissive about unaligned memory accesses. This may not be the case for
    // the host platform, so we do the equivalent of a memcpy from the VM's memory before
    // trying to interpret the value. Most compilers will detect what we're doing and
    // optimize it away.
    uint16_t v;
    std::ranges::copy_n(memory.data() + addr, sizeof(v), reinterpret_cast<std::byte*>(&v));

    // Owl-2820 is little-endian, so swap the byte order if necessary.
    return AsLE(v);
}

inline uint32_t Read32(const Memory memory, uint32_t addr)
{
    if (!IsWritable<4>(addr) && !IsReadable<4>(addr))
    {
        throw std::runtime_error(std::format("Read32: Segmentation violation: 0x{:08x}", addr));
    }

    // Owl-2820 is permissive about unaligned memory accesses. This may not be the case for
    // the host platform, so we do the equivalent of a memcpy from the VM's memory before
    // trying to interpret the value. Most compilers will detect what we're doing and
    // optimize it away.
    uint32_t v;
    std::ranges::copy_n(memory.data() + addr, sizeof(uint32_t), reinterpret_cast<std::byte*>(&v));

    // Owl-2820 is little-endian, so swap the byte order if necessary.
    return AsLE(v);
}

inline void Write8(Memory memory, uint32_t addr, std::byte byte)
{
    if (!IsWritable<1>(addr))
    {
        throw std::runtime_error(std::format("Write8: Segmentation violation: 0x{:08x}", addr));
    }
    memory[addr] = byte;
}

inline void Write16(Memory memory, uint32_t addr, uint16_t halfWord)
{
    if (!IsWritable<2>(addr))
    {
        throw std::runtime_error(std::format("Write16: Segmentation violation: 0x{:08x}", addr));
    }

    // Owl-2820 is little-endian, so swap the byte order if necessary.
    const uint16_t v = AsLE(halfWord);

    // Owl-2820 is permissive about unaligned memory accesses. This may not be the case for
    // the host platform, so we do the equivalent of a memcpy when writing the value to the
    // VM's memory. Most compilers will detect what we're doing and optimize it away.
    std::ranges::copy_n(reinterpret_cast<const std::byte*>(&v), sizeof(v), memory.data() + addr);
}

inline void Write32(Memory memory, uint32_t addr, uint32_t word)
{
    if (!IsWritable<4>(addr))
    {
        throw std::runtime_error(std::format("Write32: Segmentation violation: 0x{:08x}", addr));
    }

    // Owl-2820 is little-endian, so swap the byte order if necessary.
    const uint32_t v = AsLE(word);

    // Owl-2820 is permissive about unaligned memory accesses. This may not be the case for
    // the host platform, so we do the equivalent of a memcpy when writing the value to the
    // VM's memory. Most compilers will detect what we're doing and optimize it away.
    std::ranges::copy_n(reinterpret_cast<const std::byte*>(&v), sizeof(v), memory.data() + addr);
}
