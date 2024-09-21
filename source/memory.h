#pragma once

#include "endian.h"

#include <algorithm>
#include <cstddef>
#include <span>

using Memory = std::span<std::byte>;

inline std::byte Read8(const Memory memory, uint32_t addr)
{
    return memory[addr];
}

inline std::uint16_t Read16(const Memory memory, uint32_t addr)
{
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
    memory[addr] = byte;
}

inline void Write16(Memory memory, uint32_t addr, uint16_t halfWord)
{
    // Owl-2820 is little-endian, so swap the byte order if necessary.
    const uint16_t v = AsLE(halfWord);

    // Owl-2820 is permissive about unaligned memory accesses. This may not be the case for
    // the host platform, so we do the equivalent of a memcpy when writing the value to the
    // VM's memory. Most compilers will detect what we're doing and optimize it away.
    std::ranges::copy_n(reinterpret_cast<const std::byte*>(&v), sizeof(v), memory.data() + addr);
}

inline void Write32(Memory memory, uint32_t addr, uint32_t word)
{
    // Owl-2820 is little-endian, so swap the byte order if necessary.
    const uint32_t v = AsLE(word);

    // Owl-2820 is permissive about unaligned memory accesses. This may not be the case for
    // the host platform, so we do the equivalent of a memcpy when writing the value to the
    // VM's memory. Most compilers will detect what we're doing and optimize it away.
    std::ranges::copy_n(reinterpret_cast<const std::byte*>(&v), sizeof(uint32_t),
                        memory.data() + addr);
}
