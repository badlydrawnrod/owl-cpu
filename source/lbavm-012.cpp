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
#include <limits>
#include <span>
#include <vector>

constexpr size_t memorySize = 0x8000;

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

struct Image
{
    std::vector<uint32_t> image;
    uint32_t textStart;
    uint32_t textSize;
};

elf::Result<Image> LoadElfImage(const char* filename)
{
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);
    auto fileSize = ifs.tellg();
    if (!ifs.seekg(0))
    {
        return std::unexpected(elf::Error::ioFailed);
    }

    // The memory that we're going to sub-allocate.
    std::vector<uint32_t> image(memorySize / sizeof(uint32_t));

    // The world's most rudimentary segment allocator.
    auto Allocate = [&](const elf::Phdr& phdr) -> elf::MemoryResult {
        const auto paddr = phdr.p_paddr;
        const auto memsz = phdr.p_memsz;
        auto dst = std::as_writable_bytes(std::span(image));
        if (paddr + memsz > dst.size())
        {
            return std::unexpected(elf::Error::invalidArgument);
        }

        return dst.subspan(paddr, memsz);
    };

    auto segments = elf::LoadExecutable(ifs, fileSize, Allocate);
    if (!segments)
    {
        return std::unexpected(segments.error());
    }

    const uint32_t textStart = segments->lma.startText / sizeof(uint32_t);
    const uint32_t textSize = (segments->lma.endText - segments->lma.startText) / sizeof(uint32_t);

    return Image{.image = image, .textStart = textStart, .textSize = textSize};
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

        // Load the image from an ELF file.
        auto loadedImage = LoadElfImage(argv[1]);
        if (!loadedImage)
        {
            std::cerr << "Failed to load ELF image: " << loadedImage.error() << '\n';
            return 1;
        }

        // Find the code in the loaded image.
        auto& rv32iImage = loadedImage->image;
        std::span<uint32_t> rv32iText(rv32iImage.begin() + loadedImage->textStart,
                                      loadedImage->textSize);

        std::cout << "Disassembling RISC-V encoded instructions...\n";
        DisassembleRv32i(rv32iText);

        // Create a memory image.
        std::vector<uint32_t> image(memorySize / sizeof(uint32_t));

        // Copy the entire loaded image into the memory image.
        std::ranges::copy(rv32iImage, image.begin());

        std::cout << "Running RISC-V encoded instructions...\n";
        RunRv32i(image);

        // Transcode the code part of the image to Owl-2820 encoding.
        auto owlText = Rv32iToOwl(rv32iText);

        std::cout << "Disassembling Owl-2820 encoded instructions...\n";
        DisassembleOwl(owlText);

        // Copy the entire loaded image into the memory image again to re-initialize .data and
        // .sdata for non-flash images.
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
