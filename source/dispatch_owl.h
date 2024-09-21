#pragma once

#include "cpu.h"
#include "opcodes.h"

#include <cstdint>

namespace decode
{
    inline uint32_t r0(const uint32_t ins)
    {
        return (ins >> 7) & 0x1f;
    }

    inline uint32_t r1(const uint32_t ins)
    {
        return (ins >> 12) & 0x1f;
    }

    inline uint32_t r2(const uint32_t ins)
    {
        return (ins >> 17) & 0x1f;
    }

    inline uint32_t shift(const uint32_t ins)
    {
        return (ins >> 17) & 0x1f;
    }

    inline uint32_t imm12(const uint32_t ins)
    {
        return static_cast<uint32_t>(static_cast<int32_t>(ins & 0xfff00000) >> 20);
    }

    inline uint32_t offs12(const uint32_t ins)
    {
        return static_cast<uint32_t>(static_cast<int32_t>(ins & 0xfff00000) >> 19);
    }

    inline uint32_t offs20(const uint32_t ins)
    {
        return static_cast<uint32_t>(static_cast<int32_t>(ins & 0xfffff000) >> 11);
    }

    inline uint32_t uimm20(const uint32_t ins)
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
