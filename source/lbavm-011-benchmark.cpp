#include "assembler.h"
#include "cpu.h"
#include "dispatch_owl.h"
#include "dispatch_rv32i.h"
#include "endian.h"
#include "memory.h"
#include "opcodes.h"

#include <chrono>
#include <cstdint>
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

void Transcode(Assembler& a, uint32_t code)
{
    DecodeRv32 rv(code);

    // clang-format off
    switch (code) {
        case 0x00000073: return a.Ecall();
        case 0x00100073: return a.Ebreak();
    }
    switch (code & 0xfe00707f) {
        case 0x00000033: return a.Add(rv.Rd(), rv.Rs1(), rv.Rs2());
        case 0x40000033: return a.Sub(rv.Rd(), rv.Rs1(), rv.Rs2());
        case 0x00001033: return a.Sll(rv.Rd(), rv.Rs1(), rv.Rs2());
        case 0x00002033: return a.Slt(rv.Rd(), rv.Rs1(), rv.Rs2());
        case 0x00003033: return a.Sltu(rv.Rd(), rv.Rs1(), rv.Rs2());
        case 0x00004033: return a.Xor(rv.Rd(), rv.Rs1(), rv.Rs2());
        case 0x00005033: return a.Srl(rv.Rd(), rv.Rs1(), rv.Rs2());
        case 0x40005033: return a.Sra(rv.Rd(), rv.Rs1(), rv.Rs2());
        case 0x00006033: return a.Or(rv.Rd(), rv.Rs1(), rv.Rs2());
        case 0x00007033: return a.And(rv.Rd(), rv.Rs1(), rv.Rs2());
        case 0x00001013: return a.Slli(rv.Rd(), rv.Rs1(), rv.Shamtw());
        case 0x00005013: return a.Srli(rv.Rd(), rv.Rs1(), rv.Shamtw());
        case 0x40005013: return a.Srai(rv.Rd(), rv.Rs1(), rv.Shamtw());
    }
    switch (code & 0x0000707f) {
        case 0x00000063: return a.Beq(rv.Rs1(), rv.Rs2(), rv.Bimmediate());
        case 0x00001063: return a.Bne(rv.Rs1(), rv.Rs2(), rv.Bimmediate());
        case 0x00004063: return a.Blt(rv.Rs1(), rv.Rs2(), rv.Bimmediate());
        case 0x00005063: return a.Bge(rv.Rs1(), rv.Rs2(), rv.Bimmediate());
        case 0x00006063: return a.Bltu(rv.Rs1(), rv.Rs2(), rv.Bimmediate());
        case 0x00007063: return a.Bgeu(rv.Rs1(), rv.Rs2(), rv.Bimmediate());
        case 0x00000067: return a.Jalr(rv.Rd(), rv.Iimmediate(),rv.Rs1()); // changed order
        case 0x00000013: return a.Addi(rv.Rd(), rv.Rs1(), rv.Iimmediate());
        case 0x00002013: return a.Slti(rv.Rd(), rv.Rs1(), rv.Iimmediate());
        case 0x00003013: return a.Sltiu(rv.Rd(), rv.Rs1(), rv.Iimmediate());
        case 0x00004013: return a.Xori(rv.Rd(), rv.Rs1(), rv.Iimmediate());
        case 0x00006013: return a.Ori(rv.Rd(), rv.Rs1(), rv.Iimmediate());
        case 0x00007013: return a.Andi(rv.Rd(), rv.Rs1(), rv.Iimmediate());
        case 0x00000003: return a.Lb(rv.Rd(), rv.Iimmediate(), rv.Rs1()); // changed order
        case 0x00001003: return a.Lh(rv.Rd(), rv.Iimmediate(), rv.Rs1()); // changed order
        case 0x00002003: return a.Lw(rv.Rd(), rv.Iimmediate(), rv.Rs1()); // changed order
        case 0x00004003: return a.Lbu(rv.Rd(), rv.Iimmediate(), rv.Rs1()); // changed order
        case 0x00005003: return a.Lhu(rv.Rd(), rv.Iimmediate(), rv.Rs1()); // changed order
        case 0x00000023: return a.Sb(rv.Rs1(), rv.Simmediate(), rv.Rs2()); // changed order
        case 0x00001023: return a.Sh(rv.Rs1(), rv.Simmediate(), rv.Rs2()); // changed order
        case 0x00002023: return a.Sw(rv.Rs1(), rv.Simmediate(), rv.Rs2()); // changed order
        case 0x0000000f: return a.Fence();
    }
    switch (code & 0x0000007f) {
        case 0x0000006f: return a.Jal(rv.Rd(), rv.Jimmediate());
        case 0x00000037: return a.Lui(rv.Rd(), rv.Uimmediate());
        case 0x00000017: return a.Auipc(rv.Rd(), rv.Uimmediate());
    }
    // clang-format on
    return a.Illegal(code);
}

std::vector<uint32_t> Rv32iToOwl(std::span<uint32_t> image)
{
    Assembler a;
    for (auto code : image)
    {
        Transcode(a, code);
    }
    return a.Code();
}

std::span<uint32_t> LoadRv32iImage()
{
    // The RISC-V binary image from: https://badlydrawnrod.github.io/posts/2024/08/20/lbavm-008/
    // clang-format off
    alignas(alignof(uint32_t)) static uint8_t image[] = {
        0x13, 0x05, 0x00, 0x00, 0x93, 0x05, 0x00, 0x00, 0x13, 0x06, 0x00, 0x00, 0xEF, 0x00, 0x40, 0x0F, // 00000000   13 05 00 00 93 05 00 00 13 06 00 00 EF 00 40 0F
        0x13, 0x05, 0x00, 0x00, 0x93, 0x08, 0x00, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00000010   13 05 00 00 93 08 00 00 73 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00000020   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00000030   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00000040   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00000050   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00000060   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00000070   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00000080   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 00000090   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000000A0   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000000B0   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000000C0   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000000D0   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000000E0   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 000000F0   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        0x13, 0x06, 0x00, 0x00, 0x93, 0x06, 0x20, 0x00, 0x13, 0x07, 0x10, 0x00, 0x93, 0x07, 0x00, 0x03, // 00000100   13 06 00 00 93 06 20 00 13 07 10 00 93 07 00 03
        0x93, 0x05, 0x06, 0x00, 0x63, 0x62, 0xD6, 0x02, 0x13, 0x05, 0x00, 0x00, 0x93, 0x05, 0x10, 0x00, // 00000110   93 05 06 00 63 62 D6 02 13 05 00 00 93 05 10 00
        0x13, 0x08, 0x06, 0x00, 0x93, 0x88, 0x05, 0x00, 0x13, 0x08, 0xF8, 0xFF, 0xB3, 0x05, 0xB5, 0x00, // 00000120   13 08 06 00 93 88 05 00 13 08 F8 FF B3 05 B5 00
        0x13, 0x85, 0x08, 0x00, 0xE3, 0x68, 0x07, 0xFF, 0x93, 0x08, 0x10, 0x00, 0x13, 0x05, 0x06, 0x00, // 00000130   13 85 08 00 E3 68 07 FF 93 08 10 00 13 05 06 00
        0x73, 0x00, 0x00, 0x00, 0x13, 0x06, 0x16, 0x00, 0xE3, 0x14, 0xF6, 0xFC, 0x13, 0x05, 0x00, 0x00, // 00000140   73 00 00 00 13 06 16 00 E3 14 F6 FC 13 05 00 00
        0x67, 0x80, 0x00, 0x00                                                                          // 00000150   67 80 00 00
    };
    // clang-format on

    uint32_t* imageBegin = reinterpret_cast<uint32_t*>(image);
    uint32_t* imageEnd = imageBegin + sizeof(image) / sizeof(uint32_t);
    return std::span<uint32_t>(imageBegin, imageEnd);
}

int main()
{
    try
    {
        // Create a 4K memory image.
        constexpr size_t memorySize = 4096;
        std::vector<uint32_t> image(memorySize / sizeof(uint32_t));

        auto rv32iImage = LoadRv32iImage();

        // Crude microbenchmark.
        constexpr int attempts = 1'000'000;

        // Copy the result into our VM image to run it directly.
        std::ranges::copy(rv32iImage, image.begin());
        const auto startRv32i{std::chrono::steady_clock::now()};
        for (int i = 0; i < attempts; i++)
        {
            RunRv32i(image);
        }
        const auto endRv32i{std::chrono::steady_clock::now()};

        // Transcode it to Owl-2820 and copy the result into our VM image.
        auto owlImage = Rv32iToOwl(rv32iImage);
        std::ranges::copy(owlImage, image.begin());

        // Run it as Owl-2820.
        const auto startOwl{std::chrono::steady_clock::now()};
        for (int i = 0; i < attempts; i++)
        {
            Run(image);
        }
        const auto endOwl{std::chrono::steady_clock::now()};

        const std::chrono::duration<double> elapsedRv32i{endRv32i - startRv32i};
        const std::chrono::duration<double> elapsedOwl{endOwl - startOwl};

        std::cout << "Elapsed Rv32i: " << elapsedRv32i << '\n';
        std::cout << "Elapsed   Owl: " << elapsedOwl << "\n\n";
        std::cout << "RV32I timing as percentage of Owl: " << 100.0 * (elapsedRv32i / elapsedOwl) << '\n';
        std::cout << "Owl timing as percentage of RV32I: " << 100.0 * (elapsedOwl / elapsedRv32i) << '\n';
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
