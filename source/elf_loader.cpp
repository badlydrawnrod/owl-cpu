#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <span>
#include <system_error>

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

// Section header (Shdr).
struct Elf32_Shdr
{
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};

// All headers.
struct Elf32_Headers
{
    Elf32_Ehdr ehdr;               // ELF header.
    std::vector<Elf32_Phdr> phdrs; // All program headers.
    std::vector<Elf32_Shdr> shdrs; // All section headers.
    std::vector<char> names;       // The string section.
};

enum class elf_errc
{
    ER_OK = 0,           // The ELF file was loaded successfully.
    ER_BAD_ELF,          // The ELF file is badly formatted in some way.
    ER_INVALID_ARGUMENT, // The caller passed a bad argument.
    ER_IO_FAILED,        // An I/O operation failed while reading the ELF file.
    ER_NOT_SUPPORTED,    // The loader doesn't support some aspect of the ELF file, e.g., it isn't
                         // RISC-V.
};

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

Elf32_Ehdr ReadElfHeader(std::ifstream& ifs, uint32_t fileSize, elf_errc& err)
{
    // Read the ELF header.
    Elf32_Ehdr ehdr{};

    ifs.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
    if (!ifs)
    {
        err = elf_errc::ER_IO_FAILED;
        return {};
    }

    // Check the magic number.
    const auto& e_ident = ehdr.e_ident;
    if (e_ident[EI_MAG0] != '\x7f' || e_ident[EI_MAG1] != 'E' || e_ident[EI_MAG2] != 'L'
        || e_ident[EI_MAG3] != 'F')
    {
        err = elf_errc::ER_BAD_ELF;
        return {};
    }

    // Check that it is 32-bit.
    if (e_ident[EI_CLASS] != 1)
    {
        err = elf_errc::ER_NOT_SUPPORTED;
        return {};
    }

    // Check that it is two's complement, little-endian.
    if (e_ident[EI_DATA] != 1)
    {
        err = elf_errc::ER_NOT_SUPPORTED;
        return {};
    }

    // Check the ident version.
    if (e_ident[EI_VERSION] != 1)
    {
        err = elf_errc::ER_NOT_SUPPORTED;
        return {};
    }

    // Check that it is an executable.
    if (ehdr.e_type != ET_EXEC)
    {
        err = elf_errc::ER_NOT_SUPPORTED;
        return {};
    }

    // Check that it is for RISC-V.
    if (ehdr.e_machine != EM_RISCV)
    {
        err = elf_errc::ER_NOT_SUPPORTED;
        return {};
    }

    // Check the version.
    if (ehdr.e_version != 1)
    {
        err = elf_errc::ER_NOT_SUPPORTED;
        return {};
    }

    // Check that the size of a program header entry is what we expect it to be.
    if (ehdr.e_phentsize != sizeof(Elf32_Phdr))
    {
        err = elf_errc::ER_BAD_ELF;
        return {};
    }

    // Check that the program header table is beyond the ELF header and within the file.
    // TODO: overflow, overlap.
    if (ehdr.e_phoff < sizeof(ehdr) || ehdr.e_phoff + ehdr.e_phentsize * ehdr.e_phnum > fileSize)
    {
        err = elf_errc::ER_BAD_ELF;
        return {};
    }

    // Check that the size of a section header entry is what we expect it to be.
    if (ehdr.e_shentsize != sizeof(Elf32_Shdr))
    {
        err = elf_errc::ER_BAD_ELF;
        return {};
    }

    // Check that the section header table is beyond the ELF header and within the file.
    // TODO: overflow, overlap.
    if (ehdr.e_shoff < sizeof(ehdr) || ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shnum > fileSize)
    {
        err = elf_errc::ER_BAD_ELF;
        return {};
    }

    // Check that the string section is in bounds.
    if (ehdr.e_shstrndx >= ehdr.e_shnum)
    {
        err = elf_errc::ER_BAD_ELF;
        return {};
    }

    return ehdr;
}

Elf32_Phdr ReadProgramHeader(std::ifstream& ifs, uint32_t fileSize, elf_errc& err)
{
    // Read a program header.
    Elf32_Phdr phdr{};
    ifs.read(reinterpret_cast<char*>(&phdr), sizeof(phdr));
    if (!ifs)
    {
        err = elf_errc::ER_IO_FAILED;
        return {};
    }

    // The file size may not be larger than the memory size.
    if (phdr.p_filesz > phdr.p_memsz)
    {
        err = elf_errc::ER_BAD_ELF;
        return {};
    }

    // TODO: other validation.

    return phdr;
}

Elf32_Shdr ReadSectionHeader(std::ifstream& ifs, uint32_t fileSize, elf_errc& err)
{
    // Read a section header.
    Elf32_Shdr shdr{};
    ifs.read(reinterpret_cast<char*>(&shdr), sizeof(shdr));
    if (!ifs)
    {
        err = elf_errc::ER_IO_FAILED;
        return {};
    }

    // TODO: other validation.

    return shdr;
}

void ReadSegment(std::ifstream& ifs, const Elf32_Phdr& phdr, std::span<char> dst, elf_errc& err)
{
    // Is the destination big enough?
    if (phdr.p_memsz > dst.size_bytes())
    {
        err = elf_errc::ER_INVALID_ARGUMENT;
        return;
    }

    // Zero the destination up to p_memsz.
    std::ranges::fill_n(dst.begin(), phdr.p_memsz, '\0');

    // Copy data up to p_filesz (p_filesz <= p_memsz).
    if (phdr.p_filesz != 0)
    {
        ifs.seekg(phdr.p_offset);
        ifs.read(dst.data(), phdr.p_filesz);
    }
}

void ReadSection(std::ifstream& ifs, const Elf32_Shdr& shdr, std::span<char> dst, elf_errc& err)
{
    // Is the destination big enough?
    if (shdr.sh_size > dst.size_bytes())
    {
        err = elf_errc::ER_INVALID_ARGUMENT;
        return;
    }

    // Zero the destination up to sh_size.
    std::ranges::fill_n(dst.begin(), shdr.sh_size, '\0');

    // Copy data up to sh_size.
    if (shdr.sh_size != 0 && shdr.sh_type != SHT_NOBITS)
    {
        ifs.seekg(shdr.sh_offset);
        ifs.read(dst.data(), shdr.sh_size);
    }
}

Elf32_Headers ReadElf(std::ifstream& ifs, uint32_t fileSize, elf_errc& err)
{
    // Read the ELF header.
    Elf32_Ehdr ehdr = ReadElfHeader(ifs, fileSize, err);
    if (err != elf_errc::ER_OK)
    {
        return {};
    }

    // Read the program headers.
    std::vector<Elf32_Phdr> phdrs;
    phdrs.reserve(ehdr.e_phnum);
    for (uint16_t i = 0; i < ehdr.e_phnum; i++)
    {
        // Go to the program header's entry in the program header table.
        ifs.seekg(ehdr.e_phoff + i * ehdr.e_phentsize);
        if (!ifs)
        {
            err = elf_errc::ER_IO_FAILED;
            return {};
        }

        Elf32_Phdr phdr = ReadProgramHeader(ifs, fileSize, err);
        if (err != elf_errc::ER_OK)
        {
            return {};
        }
        phdrs.push_back(phdr);
    }

    // Read the section headers.
    std::vector<Elf32_Shdr> shdrs;
    shdrs.reserve(ehdr.e_shnum);
    for (uint16_t i = 0; i < ehdr.e_shnum; i++)
    {
        // Go to the section header's entry in the program header table.
        ifs.seekg(ehdr.e_shoff + i * ehdr.e_shentsize);
        if (!ifs)
        {
            err = elf_errc::ER_IO_FAILED;
            return {};
        }

        Elf32_Shdr shdr = ReadSectionHeader(ifs, fileSize, err);
        if (err != elf_errc::ER_OK)
        {
            return {};
        }
        shdrs.push_back(shdr);
    }

    // Find the section header for the string section.
    const Elf32_Shdr& stringSection = shdrs[ehdr.e_shstrndx];
    const auto size = stringSection.sh_size;

    // Check that string indexes are within the string section.
    for (const auto& shdr : shdrs)
    {
        if (shdr.sh_name >= size)
        {
            err = elf_errc::ER_BAD_ELF;
            return {};
        }
    }

    // Load the string section.
    std::vector<char> names(size);
    ReadSection(ifs, stringSection, names, err);
    if (err != elf_errc::ER_OK)
    {
        return {};
    }

    Elf32_Headers headers{.ehdr = ehdr, .phdrs = phdrs, .shdrs = shdrs, .names = names};
    return headers;
}

class ElfLoader
{
public:
    ElfLoader(const char* path) : ifs_(path, std::ios::binary | std::ios::ate)
    {
        auto fileSize = ifs_.tellg();
        ifs_.seekg(0);
        if (!ifs_)
        {
            err_ = elf_errc::ER_IO_FAILED;
            return;
        }

        headers_ = ReadElf(ifs_, fileSize, err_);
    }

    const Elf32_Ehdr& ElfHeader() const
    {
        return headers_.ehdr;
    }

    const std::vector<Elf32_Phdr>& ProgramHeaders() const
    {
        return headers_.phdrs;
    }

    const std::vector<Elf32_Shdr>& SectionHeaders() const
    {
        return headers_.shdrs;
    }

    void ReadSegment(size_t num, std::span<char> dst, elf_errc& err)
    {
        if (num >= headers_.phdrs.size())
        {
            err = elf_errc::ER_INVALID_ARGUMENT;
            return;
        }

        const Elf32_Phdr& phdr = headers_.phdrs[num];
        ::ReadSegment(ifs_, phdr, dst, err);
    }

    void ReadSection(size_t num, std::span<char> dst, elf_errc& err)
    {
        if (num >= headers_.shdrs.size())
        {
            err = elf_errc::ER_INVALID_ARGUMENT;
            return;
        }

        const Elf32_Shdr& shdr = headers_.shdrs[num];
        ::ReadSection(ifs_, shdr, dst, err);
    }

private:
    elf_errc err_{}; // TODO: decide how to handle errors once and for all.
    std::ifstream ifs_;
    Elf32_Headers headers_{};
};

int main()
{
    const char path[] = "../../target/out/a.out";

    std::ifstream elfStream(path, std::ios::binary | std::ios::ate);
    if (!elfStream)
    {
        std::cout << "Couldn't open: " << path << '\n';
        return 1;
    }
    auto fileSize = elfStream.tellg();
    elfStream.seekg(0);

    elf_errc err{};
    Elf32_Headers headers = ReadElf(elfStream, fileSize, err);
    if (err != elf_errc::ER_OK)
    {
        std::cout << to_string(err) << '\n';
        return 1;
    }

    // Display the program headers.
    std::cout << "Program Headers:\n";
    for (const auto& phdr : headers.phdrs)
    {
        if (phdr.p_type == PT_LOAD)
        {
            // Classify it based on flags and sizes.
            std::string type{};
            const auto& flags = phdr.p_flags;
            if (flags == (PF_R | PF_X))
            {
                type = "TEXT (r-x)";
            }
            else if (flags == PF_R && phdr.p_filesz != 0)
            {
                type = "RODATA (r--)";
            }
            else if (flags == (PF_R | PF_W) && phdr.p_filesz != 0)
            {
                type = "DATA (rw-)";
            }
            else if (flags == (PF_R | PF_W) && phdr.p_filesz == 0)
            {
                type = "BSS (rw-)";
            }
            else
            {
                type = "UNKNOWN";
            }

            std::cout << std::format("Found loadable {} segment\n", type);
            std::cout << std::format("\tp_offset = {:08x}\n", phdr.p_offset);
            std::cout << std::format("\t p_paddr = {:08x}\n", phdr.p_paddr);
            std::cout << std::format("\t p_vaddr = {:08x}\n", phdr.p_vaddr);
            std::cout << std::format("\tp_filesz = {:08x}\n", phdr.p_filesz);
            std::cout << std::format("\t p_memsz = {:08x}\n", phdr.p_memsz);
        }
    }

    // Display the section headers.
    std::cout << "\nSection Headers:\n";
    for (const auto& shdr : headers.shdrs)
    {
        std::string_view name(headers.names.data() + shdr.sh_name);
        std::cout << "section name: " << name << '\n';
        std::cout << std::format("\t  sh_flags = {:08x}\n", shdr.sh_flags);
        std::cout << std::format("\t   sh_type = {}\n", to_string(shdr.sh_type));
        std::cout << std::format("\t   sh_addr = {:08x}\n", shdr.sh_addr);
        std::cout << std::format("\t sh_offset = {:08x}\n", shdr.sh_offset);
        std::cout << std::format("\t   sh_size = {:08x}\n", shdr.sh_size);
        std::cout << std::format("\t   sh_info = {:08x}\n", shdr.sh_info);
    }

    return 0;
}
