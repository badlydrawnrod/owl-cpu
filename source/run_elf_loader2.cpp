#include "elf_loader.h"

#include <algorithm>
#include <format>
#include <functional>
#include <iostream>
#include <string>

void PrintHeader(const Elf32_Phdr& phdr)
{
    // Classify it based on flags and sizes.
    std::string type{};
    const auto flags = phdr.p_flags;
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

using AllocatorFn = std::function<ElfResult<std::span<std::byte>>(const Elf32_Phdr& phdr)>;

int LoadElf(std::ifstream& ifs, std::streampos fileSize, AllocatorFn allocateSegment)
{
    auto ehdr = ReadElfHeader(ifs, fileSize);
    if (!ehdr)
    {
        std::cout << "Failed to read ELF header\n";
        return 1;
    }

    // Go to the start of the program header table.
    if (!ifs.seekg(ehdr->e_phoff))
    {
        std::cerr << "Failed to seek to program header table.\n";
        return 1;
    }

    // Read the program headers.
    std::vector<Elf32_Phdr> phdrs;
    phdrs.reserve(ehdr->e_phnum);
    for (uint16_t i = 0; i < ehdr->e_phnum; i++)
    {
        auto phdr = ReadProgramHeader(ifs, fileSize);
        if (!phdr)
        {
            std::cout << "Failed to read program header " << i << '\n';
            return 1;
        }
        phdrs.push_back(*phdr);
    }

    // Iterate through the program headers.
    for (const auto& phdr : phdrs)
    {
        if (phdr.p_type != PT_LOAD)
        {
            continue;
        }
        PrintHeader(phdr);

        // Get some memory for the segment.
        auto segment = allocateSegment(phdr);
        if (!segment)
        {
            std::cerr << "Failed to allocate memory for segment.\n";
            return 1;
        }

        // Zero the memory.
        std::ranges::fill(*segment, std::byte{});

        // Seek to the start of the segment data for this program header.
        if (!ifs.seekg(phdr.p_offset))
        {
            std::cerr << "Failed to seek to header.\n";
            return 1;
        }

        // Load the data into the segment.
        char* dstData = reinterpret_cast<char*>(segment->data());
        if (!ifs.read(dstData, phdr.p_filesz))
        {
            std::cerr << "Failed to copy data to segment.\n";
            return 1;
        }
    }

    return 0;
}

int main()
{
    const char path[] = "../../target/out/a.out";

    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    auto fileSize = ifs.tellg();
    if (!ifs.seekg(0))
    {
        std::cerr << "Failed to seek to end of file.\n";
        return 1;
    }

    // Segment allocator.
    std::vector<uint32_t> scratch(32768);
    auto Alloc = [&scratch](const Elf32_Phdr& phdr) -> ElfResult<std::span<std::byte>> {
        const auto paddr = phdr.p_paddr;
        const auto memsz = phdr.p_memsz;
        auto dst = std::as_writable_bytes(std::span(scratch));
        if (paddr + memsz > dst.size())
        {
            return std::unexpected(elf_errc::ER_INVALID_ARGUMENT);
        }
        return dst.subspan(paddr, memsz);
    };

    LoadElf(ifs, fileSize, Alloc);

    std::cout << "Done\n";

    return 0;
}
