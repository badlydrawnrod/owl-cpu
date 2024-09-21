#include "assembler.h"
#include "endian.h"
#include "memory.h"
#include "opcodes.h"
#include "owl-cpu.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace decode
{
    uint32_t r0(const uint32_t ins)
    {
        return (ins >> 7) & 0x1f;
    }

    uint32_t r1(const uint32_t ins)
    {
        return (ins >> 12) & 0x1f;
    }

    uint32_t r2(const uint32_t ins)
    {
        return (ins >> 17) & 0x1f;
    }

    uint32_t shift(const uint32_t ins)
    {
        return (ins >> 17) & 0x1f;
    }

    uint32_t imm12(const uint32_t ins)
    {
        return static_cast<uint32_t>(static_cast<int32_t>(ins & 0xfff00000) >> 20);
    }

    uint32_t offs12(const uint32_t ins)
    {
        return static_cast<uint32_t>(static_cast<int32_t>(ins & 0xfff00000) >> 19);
    }

    uint32_t offs20(const uint32_t ins)
    {
        return static_cast<uint32_t>(static_cast<int32_t>(ins & 0xfffff000) >> 11);
    }

    uint32_t uimm20(const uint32_t ins)
    {
        return ins & 0xfffff000;
    }
} // namespace decode

void DispatchOwl(OwlCpu& cpu, uint32_t ins)
{
    using namespace decode;

    // Decode the word to extract the opcode.
    const Opcode opcode = Opcode(ins & 0x7f);

    // Dispatch it and execute it.
    // clang-format off
    switch (opcode)
    {
        case Opcode::Ecall: return cpu.Ecall();
        case Opcode::Ebreak: return cpu.Ebreak();
        case Opcode::Add: return cpu.Add(r0(ins), r1(ins), r2(ins));
        case Opcode::Sub: return cpu.Sub(r0(ins), r1(ins), r2(ins));
        case Opcode::Sll: return cpu.Sll(r0(ins), r1(ins), r2(ins));
        case Opcode::Slt: return cpu.Slt(r0(ins), r1(ins), r2(ins));
        case Opcode::Sltu: return cpu.Sltu(r0(ins), r1(ins), r2(ins));
        case Opcode::Xor: return cpu.Xor(r0(ins), r1(ins), r2(ins));
        case Opcode::Srl: return cpu.Srl(r0(ins), r1(ins), r2(ins));
        case Opcode::Sra: return cpu.Sra(r0(ins), r1(ins), r2(ins));
        case Opcode::Or: return cpu.Or(r0(ins), r1(ins), r2(ins));
        case Opcode::And: return cpu.And(r0(ins), r1(ins), r2(ins));
        case Opcode::Slli: return cpu.Slli(r0(ins), r1(ins), shift(ins));
        case Opcode::Srli: return cpu.Srli(r0(ins), r1(ins), shift(ins));
        case Opcode::Srai: return cpu.Srai(r0(ins), r1(ins), shift(ins));
        case Opcode::Beq: return cpu.Beq(r0(ins), r1(ins), offs12(ins));
        case Opcode::Bne: return cpu.Bne(r0(ins), r1(ins), offs12(ins));
        case Opcode::Blt: return cpu.Blt(r0(ins), r1(ins), offs12(ins));
        case Opcode::Bge: return cpu.Bge(r0(ins), r1(ins), offs12(ins));
        case Opcode::Bltu: return cpu.Bltu(r0(ins), r1(ins), offs12(ins));
        case Opcode::Bgeu: return cpu.Bgeu(r0(ins), r1(ins), offs12(ins));
        case Opcode::Addi: return cpu.Addi(r0(ins), r1(ins), imm12(ins));
        case Opcode::Slti: return cpu.Slti(r0(ins), r1(ins), imm12(ins));
        case Opcode::Sltiu: return cpu.Sltiu(r0(ins), r1(ins), imm12(ins));
        case Opcode::Xori: return cpu.Xori(r0(ins), r1(ins), imm12(ins));
        case Opcode::Ori: return cpu.Ori(r0(ins), r1(ins), imm12(ins));
        case Opcode::Andi: return cpu.Andi(r0(ins), r1(ins), imm12(ins));
        case Opcode::Lb: return cpu.Lb(r0(ins), imm12(ins), r1(ins));
        case Opcode::Lbu: return cpu.Lbu(r0(ins), imm12(ins), r1(ins));
        case Opcode::Lh: return cpu.Lh(r0(ins), imm12(ins), r1(ins));
        case Opcode::Lhu: return cpu.Lhu(r0(ins), imm12(ins), r1(ins));
        case Opcode::Lw: return cpu.Lw(r0(ins), imm12(ins), r1(ins));
        case Opcode::Sb: return cpu.Sb(r0(ins), imm12(ins), r1(ins));
        case Opcode::Sh: return cpu.Sh(r0(ins), imm12(ins), r1(ins));
        case Opcode::Sw: return cpu.Sw(r0(ins), imm12(ins), r1(ins));
        case Opcode::Fence: return cpu.Fence();
        case Opcode::Jalr: return cpu.Jalr(r0(ins), offs12(ins), r1(ins));
        case Opcode::Jal: return cpu.Jal(r0(ins), offs20(ins));
        case Opcode::Lui: return cpu.Lui(r0(ins), uimm20(ins));
        case Opcode::Auipc: return cpu.Auipc(r0(ins), uimm20(ins));
        case Opcode::J: return cpu.J(offs20(ins));
        case Opcode::Call: return cpu.Call(offs20(ins));
        case Opcode::Ret: return cpu.Ret();
        case Opcode::Li: return cpu.Li(r0(ins), imm12(ins));
        case Opcode::Mv: return cpu.Mv(r0(ins), r1(ins));
        default: return cpu.Illegal(ins);
    }
    // clang-format on
}

void Run(std::span<uint32_t> image)
{
    OwlCpu cpu(image);
    while (!cpu.Done())
    {
        const uint32_t ins = cpu.Fetch();
        DispatchOwl(cpu, ins);
    }
}

class DecodeRv32
{
    uint32_t ins_{};

public:
    DecodeRv32(uint32_t ins) : ins_{ins}
    {
    }

    uint32_t Rd() const
    {
        return (ins_ >> 7) & 0x1f;
    }

    uint32_t Rs1() const
    {
        return (ins_ >> 15) & 0x1f;
    }

    uint32_t Rs2() const
    {
        return (ins_ >> 20) & 0x1f;
    }

    uint32_t Shamtw() const
    {
        return (ins_ >> 20) & 0x1f;
    }

    uint32_t Bimmediate() const
    {
        auto imm12 = static_cast<int32_t>(ins_ & 0x80000000) >> 19;     // ins[31] -> sext(imm[12])
        auto imm11 = static_cast<int32_t>((ins_ & 0x00000080) << 4);    // ins[7] -> imm[11]
        auto imm10_5 = static_cast<int32_t>((ins_ & 0x7e000000) >> 20); // ins[30:25] -> imm[10:5]
        auto imm4_1 = static_cast<int32_t>((ins_ & 0x00000f00) >> 7);   // ins[11:8]  -> imm[4:1]
        return static_cast<uint32_t>(imm12 | imm11 | imm10_5 | imm4_1);
    }

    uint32_t Iimmediate() const
    {
        return static_cast<uint32_t>(
                (static_cast<int32_t>(ins_) >> 20)); // ins[31:20] -> sext(imm[11:0])
    }

    uint32_t Simmediate() const
    {
        auto imm11_5 =
                (static_cast<int32_t>(ins_ & 0xfe000000)) >> 20; // ins[31:25] -> sext(imm[11:5])
        auto imm4_0 = static_cast<int32_t>((ins_ & 0x00000f80) >> 7); // ins[11:7]  -> imm[4:0]
        return static_cast<uint32_t>(imm11_5 | imm4_0);
    }

    uint32_t Jimmediate() const
    {
        auto imm20 = static_cast<int32_t>(ins_ & 0x80000000) >> 11;     // ins[31] -> sext(imm[20])
        auto imm19_12 = static_cast<int32_t>(ins_ & 0x000ff000);        // ins[19:12] -> imm[19:12]
        auto imm11 = static_cast<int32_t>((ins_ & 0x00100000) >> 9);    // ins[20] -> imm[11]
        auto imm10_1 = static_cast<int32_t>((ins_ & 0x7fe00000) >> 20); // ins[30:21] -> imm[10:1]
        return static_cast<uint32_t>(imm20 | imm19_12 | imm11 | imm10_1);
    }

    auto Uimmediate() const -> uint32_t
    {
        return ins_ & 0xfffff000; // ins[31:12] -> imm[31:12]
    }
};

void DispatchRv32i(OwlCpu& a, uint32_t code)
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

        // Copy the result into our VM image to run it directly.
        std::ranges::copy(rv32iImage, image.begin());

        std::cout << "Running RISC-V encoded instructions...\n";
        RunRv32i(image);

        // Transcode it to Owl-2820 and copy the result into our VM image.
        auto owlImage = Rv32iToOwl(rv32iImage);
        std::ranges::copy(owlImage, image.begin());

        std::cout << "\nRunning Owl-2820 encoded instructions...\n";
        Run(image);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
