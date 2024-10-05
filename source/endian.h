#pragma once

#include <bit>
#include <cstdint>

// This code works on little-endian and big-endian platforms only.
static_assert(std::endian::native == std::endian::little
              || std::endian::native == std::endian::big);

inline uint16_t AsLE(uint16_t halfWord)
{
    if constexpr (std::endian::native == std::endian::little)
    {
        // Nothing to do. We're on a little-endian platform, and Owl-2820 is little-endian.
        return halfWord;
    }

    if constexpr (std::endian::native == std::endian::big)
    {
        // We're on a big-endian platform so reverse the byte order as Owl-2820 is little-endian.
        // The code may look like it takes lots of instructions, but MSVC, gcc,and clang are clever
        // enough to figure out what is going on and will generate the equivalent of `bswap` (x86)
        // or `rev` (ARM).
        //
        // std::rotr(halfWord & 00ff, 8)
        //      1234 & 00ff                     == 0034
        //      0034 rotr 8                     == 3400
        //
        // std::rotl(halfWord, 8) & 00ff
        //      1234 rotl 8                     == 3412
        //      3412 & 00ff                     == 0012
        //
        // 3400 | 0012                          == 3412
        //
        return std::rotr(halfWord & 0x00ffu, 8) | (std::rotl(halfWord, 8) & 0x00ffu);
    }
}

inline uint32_t AsLE(uint32_t word)
{
    if constexpr (std::endian::native == std::endian::little)
    {
        // Nothing to do. We're on a little-endian platform, and Owl-2820 is little-endian.
        return word;
    }

    if constexpr (std::endian::native == std::endian::big)
    {
        // We're on a big-endian platform so reverse the byte order as Owl-2820 is little-endian.
        // The code may look like it takes lots of instructions, but MSVC, gcc,and clang are clever
        // enough to figure out what is going on and will generate the equivalent of `bswap` (x86)
        // or `rev` (ARM).
        //
        // std::rotr(word & 00ff00ff, 8)
        //      12345678 & 00ff00ff             == 00340078
        //      00340078 rotr 8                 == 78004500
        //
        // std::rotl(word, 8) & 00ff00ff
        //      12345678 rotl 8                 == 34567812
        //      34567812 & 00ff00ff             == 00560012
        //
        // 78004500 | 00560012                  == 78563412
        //
        return std::rotr(word & 0x00ff00ff, 8) | (std::rotl(word, 8) & 0x00ff00ff);
    }
}
