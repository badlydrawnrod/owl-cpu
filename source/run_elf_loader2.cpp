#include "elf_loader.h"

#include <algorithm>
#include <format>
#include <functional>
#include <iostream>
#include <string>

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
    std::vector<uint32_t> scratch(32768 / sizeof(uint32_t));
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

    LoadExecutable(ifs, fileSize, Alloc);

    std::cout << "Done\n";

    return 0;
}
