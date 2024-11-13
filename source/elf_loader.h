#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <functional>
#include <span>
#include <string>
#include <vector>

// ELF format references:
// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
// https://man7.org/linux/man-pages/man5/elf.5.html

constexpr size_t EI_NIDENT = 16;

using Elf32_Addr = uint32_t;
using Elf32_Off = uint32_t;

// ELF header (Ehdr).
struct Elf32_Ehdr
{
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    Elf32_Addr e_entry;
    Elf32_Off e_phoff;
    Elf32_Off e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

// Program header (Phdr).
struct Elf32_Phdr
{
    uint32_t p_type;
    Elf32_Off p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

// TODO: Needs a better name.
struct LoadAddresses
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

// TODO: Needs a better name.
struct Loaded
{
    LoadAddresses lma{}; // Where the segments are loaded to.
    LoadAddresses vma{}; // Where they are logically.
    uint32_t startRom = 0;
    uint32_t endRom = 0;
};

// Errors.
enum class elf_errc
{
    ER_OK = 0,            // The ELF file was loaded successfully.
    ER_BAD_ELF,           // The ELF file is badly formatted in some way.
    ER_INVALID_ARGUMENT,  // The caller passed a bad argument.
    ER_IO_FAILED,         // An I/O operation failed while reading the ELF file.
    ER_NOT_SUPPORTED,     // The loader doesn't support some aspect of the ELF file, e.g., it isn't
                          // RISC-V.
    ER_ALLOCATION_FAILED, // Unable to allocate memory.
};

template<typename T>
using ElfResult = std::expected<T, elf_errc>;

using AllocatorFn = std::function<ElfResult<std::span<std::byte>>(const Elf32_Phdr& phdr)>;

std::string to_string(elf_errc err);

auto ReadElfHeader(std::istream& is, uint32_t fileSize) -> ElfResult<Elf32_Ehdr>;
auto ReadProgramHeader(std::istream& is, uint32_t fileSize) -> ElfResult<Elf32_Phdr>;
auto ReadSegment(std::istream& is, const Elf32_Phdr& phdr,
                 std::span<std::byte> dst) -> ElfResult<void>;
auto LoadExecutable(std::istream& is, std::streampos fileSize,
                    AllocatorFn allocateSegment) -> ElfResult<Loaded>;
