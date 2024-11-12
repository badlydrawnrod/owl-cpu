#include "elf_loader.h"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <format>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

// ELF format references:
// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
// https://man7.org/linux/man-pages/man5/elf.5.html

#define EI_NIDENT 16

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6

#define ET_EXEC 2
#define EM_RISCV 0xf3

#define PT_LOAD 1

#define PF_X 1
#define PF_W 2
#define PF_R 4

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_NOBITS 8
#define SHT_RISCV_ATTRIBUTES 0x70000003

#define SHF_WRITE 0x01
#define SHF_ALLOC 0x02
#define SHF_EXECINST 0x04

std::string to_string(elf_errc err)
{
    using enum elf_errc;
    switch (err)
    {
    case ER_OK:
        return "ok";
    case ER_BAD_ELF:
        return "badly formatted ELF file";
    case ER_INVALID_ARGUMENT:
        return "invalid argument";
    case ER_IO_FAILED:
        return "i/o failed";
    case ER_NOT_SUPPORTED:
        return "not supported";
    case ER_ALLOCATION_FAILED:
        return "allocation failed";
    }
    return "unknown";
}

auto ReadElfHeader(std::istream& is, uint32_t fileSize) -> ElfResult<Elf32_Ehdr>
{
    // Read the ELF header.
    Elf32_Ehdr ehdr{};

    if (!is.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr)))
    {
        return std::unexpected(elf_errc::ER_IO_FAILED);
    }

    // Check the magic number.
    const auto& e_ident = ehdr.e_ident;
    if (e_ident[EI_MAG0] != '\x7f' || e_ident[EI_MAG1] != 'E' || e_ident[EI_MAG2] != 'L'
        || e_ident[EI_MAG3] != 'F')
    {
        return std::unexpected(elf_errc::ER_BAD_ELF);
    }

    // Check that it is 32-bit.
    if (e_ident[EI_CLASS] != 1)
    {
        return std::unexpected(elf_errc::ER_NOT_SUPPORTED);
    }

    // Check that it is two's complement, little-endian.
    if (e_ident[EI_DATA] != 1)
    {
        return std::unexpected(elf_errc::ER_NOT_SUPPORTED);
    }

    // Check the ident version.
    if (e_ident[EI_VERSION] != 1)
    {
        return std::unexpected(elf_errc::ER_NOT_SUPPORTED);
    }

    // Check that it is an executable.
    if (ehdr.e_type != ET_EXEC)
    {
        return std::unexpected(elf_errc::ER_NOT_SUPPORTED);
    }

    // Check that it is for RISC-V.
    if (ehdr.e_machine != EM_RISCV)
    {
        return std::unexpected(elf_errc::ER_NOT_SUPPORTED);
    }

    // Check the version.
    if (ehdr.e_version != 1)
    {
        return std::unexpected(elf_errc::ER_NOT_SUPPORTED);
    }

    // Check that the size of a program header entry is what we expect it to be.
    if (ehdr.e_phentsize != sizeof(Elf32_Phdr))
    {
        return std::unexpected(elf_errc::ER_NOT_SUPPORTED);
    }

    // Check that the program header table is beyond the ELF header and within the file.
    // TODO: overflow, overlap.
    if (ehdr.e_phoff < sizeof(ehdr) || ehdr.e_phoff + ehdr.e_phentsize * ehdr.e_phnum > fileSize)
    {
        return std::unexpected(elf_errc::ER_BAD_ELF);
    }

    return ehdr;
}

auto ReadProgramHeader(std::istream& is, uint32_t fileSize) -> ElfResult<Elf32_Phdr>
{
    // Read a program header.
    Elf32_Phdr phdr{};
    if (!is.read(reinterpret_cast<char*>(&phdr), sizeof(phdr)))
    {
        return std::unexpected(elf_errc::ER_IO_FAILED);
    }

    // The file size may not be larger than the memory size.
    if (phdr.p_filesz > phdr.p_memsz)
    {
        return std::unexpected(elf_errc::ER_BAD_ELF);
    }

    // TODO: other validation.

    return phdr;
}

auto ReadSegment(std::istream& is, const Elf32_Phdr& phdr,
                 std::span<std::byte> dst) -> ElfResult<void>
{
    // Is the destination big enough?
    if (phdr.p_memsz > dst.size_bytes())
    {
        return std::unexpected(elf_errc::ER_INVALID_ARGUMENT);
    }

    // Zero the destination up to p_memsz.
    std::ranges::fill_n(dst.begin(), phdr.p_memsz, std::byte{});

    // Copy data up to p_filesz (p_filesz <= p_memsz).
    if (phdr.p_filesz != 0)
    {
        char* dstData = reinterpret_cast<char*>(dst.data());
        if (!is.seekg(phdr.p_offset) || !is.read(dstData, phdr.p_filesz))
        {
            return std::unexpected(elf_errc::ER_IO_FAILED);
        }
    }

    return {};
}

auto LoadExecutable(std::istream& is, std::streampos fileSize,
                    AllocatorFn allocateSegment) -> ElfResult<Loaded>
{
    auto ehdr = ReadElfHeader(is, fileSize);
    if (!ehdr)
    {
        return std::unexpected(ehdr.error());
    }

    // Go to the start of the program header table.
    if (!is.seekg(ehdr->e_phoff))
    {
        return std::unexpected(elf_errc::ER_IO_FAILED);
    }

    // Read the program headers.
    std::vector<Elf32_Phdr> phdrs;
    phdrs.reserve(ehdr->e_phnum);
    for (uint16_t i = 0; i < ehdr->e_phnum; i++)
    {
        auto phdr = ReadProgramHeader(is, fileSize);
        if (!phdr)
        {
            return std::unexpected(phdr.error());
        }
        if (phdr->p_type == PT_LOAD)
        {
            phdrs.push_back(*phdr);
        }
    }

    LoadAddresses lma{};
    LoadAddresses vma{};
    uint32_t startRom = 0;
    uint32_t endRom = 0;

    // Load each loadable segment.
    for (const auto& phdr : phdrs)
    {
        const auto paddr = phdr.p_paddr;
        const auto vaddr = phdr.p_vaddr;
        const auto memsz = phdr.p_memsz;
        const auto filesz = phdr.p_filesz;

        if (phdr.p_flags & PF_X)
        {
            // Executable segment.
            lma.startText = paddr;
            lma.endText = paddr + filesz;
            vma.startText = vaddr;
            vma.endText = vaddr + filesz;
            startRom = paddr;
            endRom = paddr + filesz;
        }
        else if (phdr.p_flags == PF_R)
        {
            // Read-only segment.
            lma.startRoData = paddr;
            lma.endRoData = paddr + filesz;
            vma.startRoData = vaddr;
            vma.endRoData = vaddr + filesz;
            endRom = paddr + filesz;
        }
        else if (phdr.p_flags == (PF_R | PF_W))
        {
            // Read/write segment.
            if (memsz == filesz)
            {
                // Initialized data only.
                lma.startData = paddr;
                lma.endData = paddr + filesz;
                vma.startData = vaddr;
                vma.endData = vaddr + filesz;
                if (paddr != vaddr)
                {
                    // The initialized data is copied from read-only memory to read-write memory.
                    endRom = paddr + filesz;
                }
            }
            else if (filesz == 0)
            {
                // Uninitialized data only.
                lma.startBss = paddr;
                lma.endBss = paddr + memsz;
                vma.startBss = vaddr;
                vma.endBss = vaddr + memsz;
            }
            else
            {
                // Both initialized data and uninitialized data.
                lma.startBss = paddr + filesz;
                lma.endBss = paddr + memsz;
                vma.startBss = vaddr;
                vma.endBss = vaddr + memsz;
                lma.startData = paddr;
                lma.endData = paddr + filesz;
                vma.startData = vaddr;
                vma.endData = vaddr + filesz;
            }
        }

        // Get some memory for the segment.
        auto segment = allocateSegment(phdr);
        if (!segment)
        {
            return std::unexpected(elf_errc::ER_IO_FAILED);
        }

        // Zero the memory.
        std::ranges::fill(*segment, std::byte{});

        // Seek to the start of the segment data for this program header.
        if (!is.seekg(phdr.p_offset))
        {
            return std::unexpected(elf_errc::ER_IO_FAILED);
        }

        // Load the segment data into the segment memory.
        char* dstData = reinterpret_cast<char*>(segment->data());
        if (!is.read(dstData, phdr.p_filesz))
        {
            return std::unexpected(elf_errc::ER_IO_FAILED);
        }
    }

    return Loaded{.lma = lma, .vma = vma, .startRom = startRom, .endRom = endRom};
}
