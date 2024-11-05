#include "elf_loader.h"

#include <cstdint>
#include <expected>
#include <format>
#include <fstream>
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
    }
    return "unknown";
}

// TODO: No. Just no. Was I asleep?
std::string to_string(uint32_t sh_type)
{
    switch (sh_type)
    {
    case SHT_NULL:
        return "SHT_NULL";
    case SHT_PROGBITS:
        return "SHT_PROGBITS";
    case SHT_SYMTAB:
        return "SHT_SYMTAB";
    case SHT_STRTAB:
        return "SHT_STRTAB";
    case SHT_NOBITS:
        return "SHT_NOBITS";
    case SHT_RISCV_ATTRIBUTES:
        return "SHT_RISCV_ATTRIBUTES";
    }
    return std::format("{:08x}", sh_type);
}

auto ReadElfHeader(std::ifstream& ifs, uint32_t fileSize) -> ElfResult<Elf32_Ehdr>
{
    // Read the ELF header.
    Elf32_Ehdr ehdr{};

    if (!ifs.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr)))
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

    // Check that the size of a section header entry is what we expect it to be.
    if (ehdr.e_shentsize != sizeof(Elf32_Shdr))
    {
        return std::unexpected(elf_errc::ER_BAD_ELF);
    }

    // Check that the section header table is beyond the ELF header and within the file.
    // TODO: overflow, overlap.
    if (ehdr.e_shoff < sizeof(ehdr) || ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shnum > fileSize)
    {
        return std::unexpected(elf_errc::ER_BAD_ELF);
    }

    // Check that the string section is in bounds.
    if (ehdr.e_shstrndx >= ehdr.e_shnum)
    {
        return std::unexpected(elf_errc::ER_BAD_ELF);
    }

    return ehdr;
}

auto ReadProgramHeader(std::ifstream& ifs, uint32_t fileSize) -> ElfResult<Elf32_Phdr>
{
    // Read a program header.
    Elf32_Phdr phdr{};
    if (!ifs.read(reinterpret_cast<char*>(&phdr), sizeof(phdr)))
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

auto ReadSectionHeader(std::ifstream& ifs, uint32_t fileSize) -> ElfResult<Elf32_Shdr>
{
    // Read a section header.
    Elf32_Shdr shdr{};
    if (!ifs.read(reinterpret_cast<char*>(&shdr), sizeof(shdr)))
    {
        return std::unexpected(elf_errc::ER_IO_FAILED);
    }

    // TODO: other validation.

    return shdr;
}

auto ReadSegment(std::ifstream& ifs, const Elf32_Phdr& phdr, std::span<char> dst) -> ElfResult<void>
{
    // Is the destination big enough?
    if (phdr.p_memsz > dst.size_bytes())
    {
        return std::unexpected(elf_errc::ER_INVALID_ARGUMENT);
    }

    // Zero the destination up to p_memsz.
    std::ranges::fill_n(dst.begin(), phdr.p_memsz, '\0');

    // Copy data up to p_filesz (p_filesz <= p_memsz).
    if (phdr.p_filesz != 0)
    {
        if (!ifs.seekg(phdr.p_offset) || !ifs.read(dst.data(), phdr.p_filesz))
        {
            return std::unexpected(elf_errc::ER_IO_FAILED);
        }
    }

    return {};
}

auto ReadSection(std::ifstream& ifs, const Elf32_Shdr& shdr, std::span<char> dst) -> ElfResult<void>
{
    // Is the destination big enough?
    if (shdr.sh_size > dst.size_bytes())
    {
        return std::unexpected(elf_errc::ER_INVALID_ARGUMENT);
    }

    // Zero the destination up to sh_size.
    std::ranges::fill_n(dst.begin(), shdr.sh_size, '\0');

    // Copy data up to sh_size.
    if (shdr.sh_size != 0 && shdr.sh_type != SHT_NOBITS)
    {
        if (!ifs.seekg(shdr.sh_offset) || !ifs.read(dst.data(), shdr.sh_size))
        {
            return std::unexpected(elf_errc::ER_IO_FAILED);
        }
    }

    return {};
}

auto ReadElf(std::ifstream& ifs, uint32_t fileSize) -> ElfResult<Elf32_Headers>
{
    // Read the ELF header.
    auto ehdr = ReadElfHeader(ifs, fileSize);
    if (!ehdr)
    {
        return std::unexpected(ehdr.error());
    }

    // Read the program headers.
    std::vector<Elf32_Phdr> phdrs;
    phdrs.reserve(ehdr->e_phnum);
    for (uint16_t i = 0; i < ehdr->e_phnum; i++)
    {
        // Go to the program header's entry in the program header table.
        if (!ifs.seekg(ehdr->e_phoff + i * ehdr->e_phentsize))
        {
            return std::unexpected(elf_errc::ER_IO_FAILED);
        }

        auto phdr = ReadProgramHeader(ifs, fileSize);
        if (!phdr)
        {
            return std::unexpected(phdr.error());
        }
        phdrs.push_back(*phdr);
    }

    // Read the section headers.
    std::vector<Elf32_Shdr> shdrs;
    shdrs.reserve(ehdr->e_shnum);
    for (uint16_t i = 0; i < ehdr->e_shnum; i++)
    {
        // Go to the section header's entry in the program header table.
        if (!ifs.seekg(ehdr->e_shoff + i * ehdr->e_shentsize))
        {
            return std::unexpected(elf_errc::ER_IO_FAILED);
        }

        auto shdr = ReadSectionHeader(ifs, fileSize);
        if (!shdr)
        {
            return std::unexpected(shdr.error());
        }
        shdrs.push_back(*shdr);
    }

    // Find the section header for the string section.
    const Elf32_Shdr& stringSection = shdrs[ehdr->e_shstrndx];
    const auto size = stringSection.sh_size;

    // Check that string indexes are within the string section.
    for (const auto& shdr : shdrs)
    {
        if (shdr.sh_name >= size)
        {
            return std::unexpected(elf_errc::ER_BAD_ELF);
        }
    }

    // Load the string section.
    std::vector<char> names(size);
    auto sectionResult = ReadSection(ifs, stringSection, names);
    if (!sectionResult)
    {
        return std::unexpected(sectionResult.error());
    }

    Elf32_Headers headers{.ehdr = *ehdr, .phdrs = phdrs, .shdrs = shdrs, .names = names};
    return headers;
}

ElfLoader::ElfLoader(const char* path) : ifs_(path, std::ios::binary | std::ios::ate)
{
    auto fileSize = ifs_.tellg();
    if (!ifs_.seekg(0))
    {
        err_ = elf_errc::ER_IO_FAILED;
        return;
    }

    auto headers = ReadElf(ifs_, fileSize);
    if (!headers)
    {
        err_ = headers.error();
        return;
    }

    headers_ = *headers;
}

auto ElfLoader::ReadSegment(size_t num, std::span<char> dst) -> ElfResult<void>
{
    if (num >= headers_.phdrs.size())
    {
        return std::unexpected(elf_errc::ER_INVALID_ARGUMENT);
    }

    const Elf32_Phdr& phdr = headers_.phdrs[num];
    return ::ReadSegment(ifs_, phdr, dst);
}

auto ElfLoader::ReadSection(size_t num, std::span<char> dst) -> ElfResult<void>
{
    if (num >= headers_.shdrs.size())
    {
        return std::unexpected(elf_errc::ER_INVALID_ARGUMENT);
    }

    const Elf32_Shdr& shdr = headers_.shdrs[num];
    return ::ReadSection(ifs_, shdr, dst);
}
