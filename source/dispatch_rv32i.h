#pragma once

#include "cpu.h"

#include <cstdint>

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
