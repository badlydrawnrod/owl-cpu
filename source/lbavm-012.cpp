#include "assembler.h"
#include "cpu.h"
#include "disassembler.h"
#include "dispatch_owl.h"
#include "dispatch_rv32i.h"
#include "endian.h"
#include "memory.h"
#include "opcodes.h"

#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

void Run(std::span<uint32_t> image)
{
    OwlCpu cpu(image);
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

void RunAndTraceRv32i(std::span<uint32_t> image)
{
    OwlCpu cpu(image);
    Disassembler d;
    while (!cpu.Done())
    {
        const uint32_t ins = cpu.Fetch();
        std::cout << std::format("{:08x}: {:20}\n", cpu.pc, DispatchRv32i(d, ins));
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

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Please supply a filename.";
            return 2;
        }

        // Create an 4K memory image.
        constexpr size_t memorySize = 4096;
        std::vector<uint32_t> image(memorySize / sizeof(uint32_t));

        auto rv32iImage = LoadRv32iImage(argv[1]);

        // TODO: we currently don't know what's code and what's data so we get garbage for some of
        // it.
        // DisassembleRv32i(rv32iImage);

        std::ranges::copy(rv32iImage, image.begin());

        std::cout << "Running RISC-V encoded instructions...\n";
        // RunRv32i(image);
        RunAndTraceRv32i(image);

        // // Transcode it to Owl-2820.
        // // TODO: we don't want to do this for the whole thing ... only for the code.
        // auto owlImage = Rv32iToOwl(rv32iImage);
        // DisassembleOwl(owlImage);
        // std::ranges::copy(owlImage, image.begin());

        // std::cout << "\nRunning Owl-2820 encoded instructions...\n";
        // Run(image);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
