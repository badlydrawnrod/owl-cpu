#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

// ELF format references:
// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
// https://man7.org/linux/man-pages/man5/elf.5.html

namespace elf
{
    constexpr size_t EI_NIDENT = 16;

    // ELF header (Ehdr).
    struct Ehdr
    {
        unsigned char e_ident[EI_NIDENT];
        uint16_t e_type;
        uint16_t e_machine;
        uint32_t e_version;
        uint32_t e_entry;
        uint32_t e_phoff;
        uint32_t e_shoff;
        uint32_t e_flags;
        uint16_t e_ehsize;
        uint16_t e_phentsize;
        uint16_t e_phnum;
        uint16_t e_shentsize;
        uint16_t e_shnum;
        uint16_t e_shstrndx;
    };

    // Program header (Phdr).
    struct Phdr
    {
        uint32_t p_type;
        uint32_t p_offset;
        uint32_t p_vaddr;
        uint32_t p_paddr;
        uint32_t p_filesz;
        uint32_t p_memsz;
        uint32_t p_flags;
        uint32_t p_align;
    };

    struct SegmentAddresses
    {
        // Code.
        uint32_t startText = 0;
        uint32_t endText = 0;
        // Read-only data.
        uint32_t startRoData = 0;
        uint32_t endRoData = 0;
        // Initialized data.
        uint32_t startData = 0;
        uint32_t endData = 0;
        // Uninitialized data.
        uint32_t startBss = 0;
        uint32_t endBss = 0;
    };

    struct Segments
    {
        uint32_t entry = 0; // The program's entry point.

        // Encompasses everything read-only, i.e., text, rodata and (on a system with flash)
        // initialized data.
        uint32_t startRom = 0;
        uint32_t endRom = 0;

        SegmentAddresses lma{}; // Where the segments are loaded to.
        SegmentAddresses vma{}; // Where they are logically.
    };

    // Errors.
    enum class Error
    {
        ok,               // The ELF file was loaded successfully.
        badElf,           // The ELF file is badly formatted in some way.
        invalidArgument,  // The caller passed a bad argument.
        ioFailed,         // An I/O operation failed while reading the ELF file.
        notSupported,     // The loader doesn't support some aspect of the ELF file, e.g., it isn't
                          // RISC-V.
        allocationFailed, // Unable to allocate memory.
    };

    template<typename T>
    using Result = std::expected<T, Error>;

    using MemorySpan = std::span<std::byte>;
    using MemoryResult = Result<MemorySpan>;

    using AllocatorFn = std::function<MemoryResult(const Phdr& phdr)>;

    std::string_view ErrorString(Error err);

    inline std::ostream& operator<<(std::ostream& os, Error err)
    {
        return os << ErrorString(err);
    }

    Result<Ehdr> ReadElfHeader(std::istream& is, uint32_t fileSize);
    Result<Phdr> ReadProgramHeader(std::istream& is, uint32_t fileSize);
    Result<void> ReadSegment(std::istream& is, const Phdr& phdr, MemorySpan dst);
    Result<Segments> LoadExecutable(std::istream& is, std::streampos fileSize,
                                    AllocatorFn allocateSegment);

}; // namespace elf
