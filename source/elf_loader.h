#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <span>
#include <string>
#include <vector>

// ELF format references:
// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
// https://man7.org/linux/man-pages/man5/elf.5.html

// TODO: Replace / hide as appropriate.

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

// Errors.
enum class elf_errc
{
    ER_OK = 0,           // The ELF file was loaded successfully.
    ER_BAD_ELF,          // The ELF file is badly formatted in some way.
    ER_INVALID_ARGUMENT, // The caller passed a bad argument.
    ER_IO_FAILED,        // An I/O operation failed while reading the ELF file.
    ER_NOT_SUPPORTED,    // The loader doesn't support some aspect of the ELF file, e.g., it isn't
                         // RISC-V.
};

template<typename T>
using ElfResult = std::expected<T, elf_errc>;

std::string to_string(elf_errc err);
std::string to_string(uint32_t sh_type);

auto ReadElfHeader(std::ifstream& ifs, uint32_t fileSize) -> ElfResult<Elf32_Ehdr>;
auto ReadProgramHeader(std::ifstream& ifs, uint32_t fileSize) -> ElfResult<Elf32_Phdr>;
auto ReadSectionHeader(std::ifstream& ifs, uint32_t fileSize) -> ElfResult<Elf32_Shdr>;
auto ReadSegment(std::ifstream& ifs, const Elf32_Phdr& phdr,
                 std::span<std::byte> dst) -> ElfResult<void>;
auto ReadSection(std::ifstream& ifs, const Elf32_Shdr& shdr,
                 std::span<std::byte> dst) -> ElfResult<void>;
auto ReadElf(std::ifstream& ifs, uint32_t fileSize) -> ElfResult<Elf32_Headers>;

class ElfLoader
{
public:
    ElfLoader(const char* path);

    operator bool() const noexcept
    {
        return err_ == elf_errc::ER_OK;
    }

    elf_errc error() const noexcept
    {
        return err_;
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

    std::string SectionName(const Elf32_Shdr& shdr) const
    {
        return headers_.names.data() + shdr.sh_name;
    }

    auto ReadSegment(size_t num, std::span<std::byte> dst) -> ElfResult<void>;
    auto ReadSection(size_t num, std::span<std::byte> dst) -> ElfResult<void>;

private:
    elf_errc err_ = elf_errc::ER_OK;
    std::ifstream ifs_{};
    Elf32_Headers headers_{};
};
