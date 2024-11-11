#include "assembler.h"
#include "cpu.h"
#include "disassembler.h"
#include "dispatch_owl.h"
#include "dispatch_rv32i.h"
#include "elf_loader.h"
#include "endian.h"
#include "memory.h"
#include "opcodes.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <span>
#include <vector>

void Run(std::span<uint32_t> image, std::span<uint32_t> text)
{
    OwlCpu cpu(image, text);
    while (!cpu.Done())
    {
        const uint32_t ins = cpu.Fetch();
        DispatchOwl(cpu, ins);
    }
}

void RunRv32i(std::span<uint32_t> image)
{
    OwlCpu cpu(image);
    while (!cpu.Done())
    {
        const uint32_t ins = cpu.Fetch();
        DispatchRv32i(cpu, ins);
    }
}

std::vector<uint32_t> Rv32iToOwl(std::span<uint32_t> image)
{
    Assembler a;
    for (auto code : image)
    {
        DispatchRv32i(a, code);
    }
    return a.Code();
}

void DisassembleOwl(std::span<uint32_t> image)
{
    Disassembler d;
    uint32_t address = 0;
    for (auto code : image)
    {
        if (code != 0)
        {
            std::cout << std::format("{:08x}: {}\n", address, DispatchOwl(d, code));
        }
        address += 4;
    }
}

void DisassembleRv32i(std::span<uint32_t> image)
{
    Disassembler d;
    uint32_t address = 0;
    for (auto code : image)
    {
        if (code != 0)
        {
            std::cout << std::format("{:08x}: {}\n", address, DispatchRv32i(d, code));
        }
        address += 4;
    }
}

std::vector<uint32_t> LoadRv32iImage(const char* filename)
{
    std::ifstream fileHandle(filename, std::ios::in | std::ios::binary | std::ios::ate);
    const std::streampos fileSize = fileHandle.tellg();
    const size_t sizeInWords = (static_cast<size_t>(fileSize) + 3) / 4;
    fileHandle.seekg(0, std::ios::beg);
    std::vector<uint32_t> buf(sizeInWords);
    fileHandle.read(reinterpret_cast<char*>(buf.data()), fileSize);
    fileHandle.close();
    return buf;
}

// TODO: return a struct rather than this mess.
std::vector<uint32_t> LoadElfImage(const char* filename, uint32_t& textStart, uint32_t& textSize)
{
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);
    auto fileSize = ifs.tellg();
    if (!ifs.seekg(0))
    {
        std::cerr << "Failed to seek to end of file.\n";
        return {};
    }

    // Segment allocator.
    constexpr size_t memorySize = 0x8000; // TODO: don't hard code.
    std::vector<uint32_t> image(memorySize / sizeof(uint32_t));
    auto Alloc = [&image, &textSize](const Elf32_Phdr& phdr) -> ElfResult<std::span<std::byte>> {
        const auto paddr = phdr.p_paddr;
        const auto memsz = phdr.p_memsz;
        auto dst = std::as_writable_bytes(std::span(image));
        if (paddr + memsz > dst.size())
        {
            return std::unexpected(elf_errc::ER_INVALID_ARGUMENT);
        }
        if (phdr.p_flags & PF_X)
        {
            textSize = memsz;
        }
        return dst.subspan(paddr, memsz);
    };

    LoadElf(ifs, fileSize, Alloc);

    auto memory = std::as_writable_bytes(std::span(image));
    return image;
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Please supply a filename.";
            return 2;
        }

        // Create a memory image.
        constexpr size_t memorySize = 0x8000; // 32K
        std::vector<uint32_t> image(memorySize / sizeof(uint32_t));

        // auto rv32iImage = LoadRv32iImage(argv[1]);
        uint32_t textStart = 0;
        uint32_t textSize = 0;
        auto rv32iImage = LoadElfImage(argv[1], textStart, textSize);

        // Find the code in the loaded image, i.e., in the TEXT sections `.init` and `.text`.
        std::span<uint32_t> rv32iText(rv32iImage.begin() + textStart / sizeof(uint32_t),
                                      textSize / sizeof(uint32_t));

        std::cout << "Disassembling RISC-V encoded instructions...\n";
        DisassembleRv32i(rv32iText);

        // Copy the entire loaded image into the memory image.
        std::ranges::copy(rv32iImage, image.begin());

        std::cout << "Running RISC-V encoded instructions...\n";
        RunRv32i(image);

        // Transcode the code part of the image to Owl-2820.
        auto owlText = Rv32iToOwl(rv32iText);

        std::cout << "Disassembling Owl-2820 encoded instructions...\n";
        DisassembleOwl(owlText);

        // Copy the entire loaded image into the memory image again (mostly to re-initialize .data
        // and .sdata for non-flash images.
        std::ranges::copy(rv32iImage, image.begin());

        std::cout << "\nRunning Owl-2820 encoded instructions...\n";
        Run(image, owlText);

        std::cout << "Done\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
