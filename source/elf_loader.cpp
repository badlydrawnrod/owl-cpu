#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>

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

#define ST_TEXT (PF_R | PF_X)
#define ST_RODATA PF_R
#define ST_DATA (PF_R | PF_W)

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
typedef struct Elf32_Shdr
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
} Elf32_Shdr;

void ReadElf(std::ifstream& ifs, uint32_t fileSize)
{
    // Read the ELF header.
    Elf32_Ehdr ehdr{};
    ifs.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));

    // --- Check the e_ident portion of the header.

    // Check the magic number.
    const auto& e_ident = ehdr.e_ident;
    if (e_ident[EI_MAG0] != '\x7f' || e_ident[EI_MAG1] != 'E' || e_ident[EI_MAG2] != 'L'
        || e_ident[EI_MAG3] != 'F')
    {
        std::cout << "Bad magic\n";
        return;
    }

    // Check that it is 32-bit.
    if (e_ident[EI_CLASS] != 1)
    {
        std::cout << "Unsupported: Not 32-bit ELF\n";
        return;
    }

    // Check that it is two's complement, little-endian.
    if (e_ident[EI_DATA] != 1)
    {
        std::cout << "Unsupported: Not two's complement, little-endian\n";
        return;
    }

    // Check the ident version.
    if (e_ident[EI_VERSION] != 1)
    {
        std::cout << "Unsupported: Unknown eident version\n";
        return;
    }

    // --- Check that it's a RISC-V executable.

    // Check that it is an executable.
    if (ehdr.e_type != ET_EXEC)
    {
        std::cout << "Unsupported: Not an executable\n";
        return;
    }

    // Check that it is for RISC-V.
    if (ehdr.e_machine != EM_RISCV)
    {
        std::cout << "Unsupported: Not for a RISC-V machine\n";
        return;
    }

    // Check the version.
    if (ehdr.e_version != 1)
    {
        std::cout << "Unsupported: Unknown ehdr version\n";
        return;
    }

    // --- Sanity check the program header table.

    // Check that the size of a program header entry is what we expect it to be.
    if (ehdr.e_phentsize != sizeof(Elf32_Phdr))
    {
        std::cout << "Unsupported: Program header size mismatch\n";
        return;
    }

    // Check that the program header table is beyond the ELF header and within the file.
    // TODO: overflow, overlap.
    if (ehdr.e_phoff < sizeof(ehdr) || ehdr.e_phoff + ehdr.e_phentsize * ehdr.e_phnum > fileSize)
    {
        std::cout << "Unsupported: Invalid program header table location\n";
        return;
    }

    // --- Sanity check the section header table.

    // Check that the size of a section header entry is what we expect it to be.
    if (ehdr.e_shentsize != sizeof(Elf32_Shdr))
    {
        std::cout << "Unsupported: Section header size mismatch\n";
        return;
    }

    // Check that the section header table is beyond the ELF header and within the file.
    // TODO: overflow, overlap.
    if (ehdr.e_shoff < sizeof(ehdr) || ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shnum > fileSize)
    {
        std::cout << "Unsupported: Invalid section header table location\n";
        return;
    }

    // At this point we know that this is an ELF file for a 32-bit RISC-V executable and its program
    // header table and section header table are at least within the file.

    for (uint16_t i = 0; i < ehdr.e_phnum; i++)
    {
        // Go to the program header's entry in the program header table.
        ifs.seekg(ehdr.e_phoff + i * ehdr.e_phentsize);

        // Read the program header.
        Elf32_Phdr phdr{};
        ifs.read(reinterpret_cast<char*>(&phdr), sizeof(phdr));

        if (phdr.p_type == PT_LOAD)
        {
            if (phdr.p_filesz > phdr.p_memsz)
            {
                // The file size may not be larget than the memory size.
                continue;
            }

            // Classify it based on flags and sizes.
            std::string type{};
            const auto& flags = phdr.p_flags;
            if (flags == (PF_R | PF_X))
            {
                type = "TEXT";
            }
            else if (flags == PF_R && phdr.p_filesz != 0)
            {
                type = "RODATA";
            }
            else if (flags == (PF_R | PF_W) && phdr.p_filesz != 0)
            {
                type = "DATA";
            }
            else if (flags == (PF_R | PF_W) && phdr.p_filesz == 0)
            {
                type = "BSS";
            }
            else
            {
                type = "UNKNOWN";
            }

            std::cout << std::format("Found loadable {} segment at {:08x}\n", type, phdr.p_offset);
            std::cout << std::format("\t p_paddr = {:08x}\n", phdr.p_paddr);
            std::cout << std::format("\t p_vaddr = {:08x}\n", phdr.p_vaddr);
            std::cout << std::format("\tp_filesz = {:08x}\n", phdr.p_filesz);
            std::cout << std::format("\t p_memsz = {:08x}\n", phdr.p_memsz);
        }
    }
}

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

    ReadElf(elfStream, fileSize);

    return 0;
}
