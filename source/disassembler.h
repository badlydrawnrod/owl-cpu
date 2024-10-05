#pragma once

#include "cpu.h"
#include "instruction_handler.h"

#include <cstdint>
#include <format>
#include <string>

class Disassembler
{
private:
    inline static const char* regnames[] = {"zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
                                            "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
                                            "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
                                            "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

public:
    using Item = std::string;

    // System instructions.
    std::string Ecall()
    {
        return "ecall";
    }

    std::string Ebreak()
    {
        return "ebreak";
    }

    // Register-register instructions.

    // add r0, r1, r2
    std::string Add(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        return std::format("add {}, {}, {}", regnames[r0], regnames[r1], regnames[r2]);
    }

    // sub r0, r1, r2
    std::string Sub(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        return std::format("sub {}, {}, {}", regnames[r0], regnames[r1], regnames[r2]);
    }

    // sll r0, r1, r2
    std::string Sll(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        return std::format("sll {}, {}, {}", regnames[r0], regnames[r1], regnames[r2]);
    }

    // slt r0, r1, r2
    std::string Slt(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        return std::format("slt {}, {}, {}", regnames[r0], regnames[r1], regnames[r2]);
    }

    // sltu r0, r1, r2
    std::string Sltu(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        return std::format("sltu {}, {}, {}", regnames[r0], regnames[r1], regnames[r2]);
    }

    // xor r0, r1, r2
    std::string Xor(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        return std::format("xor {}, {}, {}", regnames[r0], regnames[r1], regnames[r2]);
    }

    // srl r0, r1, r2
    std::string Srl(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        return std::format("srl {}, {}, {}", regnames[r0], regnames[r1], regnames[r2]);
    }

    // sra r0, r1, r2
    std::string Sra(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        return std::format("sra {}, {}, {}", regnames[r0], regnames[r1], regnames[r2]);
    }

    // or r0, r1, r2
    std::string Or(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        return std::format("or {}, {}, {}", regnames[r0], regnames[r1], regnames[r2]);
    }

    // and r0, r1, r2
    std::string And(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        return std::format("and {}, {}, {}", regnames[r0], regnames[r1], regnames[r2]);
    }

    // Immediate shift instructions.

    // slli r0, r1, shift
    std::string Slli(uint32_t r0, uint32_t r1, uint32_t shift)
    {
        return std::format("slli {}, {}, {}", regnames[r0], regnames[r1], shift);
    }

    // srli r0, r1, shift
    std::string Srli(uint32_t r0, uint32_t r1, uint32_t shift)
    {
        return std::format("srli {}, {}, {}", regnames[r0], regnames[r1], shift);
    }

    // srai r0, r1, shift
    std::string Srai(uint32_t r0, uint32_t r1, uint32_t shift)
    {
        return std::format("srai {}, {}, {}", regnames[r0], regnames[r1], shift);
    }

    // Branch instructions.

    // beq r0, r1, offs12
    std::string Beq(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        return std::format("beq {}, {}, {}", regnames[r0], regnames[r1], offs12);
    }

    // bne r0, r1, offs12
    std::string Bne(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        return std::format("bne {}, {}, {}", regnames[r0], regnames[r1], offs12);
    }

    // blt r0, r1, offs12
    std::string Blt(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        return std::format("blt {}, {}, {}", regnames[r0], regnames[r1], offs12);
    }

    // bge r0, r1, offs12
    std::string Bge(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        return std::format("bge {}, {}, {}", regnames[r0], regnames[r1], offs12);
    }

    // bltu r0, r1, offs12
    std::string Bltu(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        return std::format("bltu {}, {}, {}", regnames[r0], regnames[r1], offs12);
    }

    // bgeu r0, r1, offs12
    std::string Bgeu(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        return std::format("bgeu {}, {}, {}", regnames[r0], regnames[r1], offs12);
    }

    // Register-immediate instructions.

    // addi r0, r1, imm12
    std::string Addi(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        if (r1 == 0)
        {
            return std::format("li {}, {}", regnames[r0], imm12);
        }
        else if (imm12 == 0)
        {
            return std::format("mv {}, {}", regnames[r0], regnames[r1]);
        }
        else
        {
            return std::format("addi {}, {}, {}", regnames[r0], regnames[r1], imm12);
        }
    }

    // slti r0, r1, imm12
    std::string Slti(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        return std::format("slti {}, {}, {}", regnames[r0], regnames[r1], imm12);
    }

    // sltiu r0, r1, imm12
    std::string Sltiu(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        return std::format("sltiu {}, {}, {}", regnames[r0], regnames[r1], imm12);
    }

    // xori r0, r1, imm12
    std::string Xori(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        return std::format("xori {}, {}, {}", regnames[r0], regnames[r1], imm12);
    }

    // ori r0, r1, imm12
    std::string Ori(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        return std::format("ori {}, {}, {}", regnames[r0], regnames[r1], imm12);
    }

    // andi r0, r1, imm12
    std::string Andi(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        return std::format("andi {}, {}, {}", regnames[r0], regnames[r1], imm12);
    }

    // Load instructions.

    // lb r0, imm12(r1)
    std::string Lb(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        return std::format("lb {}, {}({})", regnames[r0], imm12, regnames[r1]);
    }

    // lbu r0, imm12(r1)
    std::string Lbu(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        return std::format("lw {}, {}({})", regnames[r0], imm12, regnames[r1]);
    }

    // lh r0, imm12(r1)
    std::string Lh(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        return std::format("lh {}, {}({})", regnames[r0], imm12, regnames[r1]);
    }

    // lhu r0, imm12(r1)
    std::string Lhu(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        return std::format("lhu {}, {}({})", regnames[r0], imm12, regnames[r1]);
    }

    // lw r0, imm12(r1)
    std::string Lw(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        return std::format("lw {}, {}({})", regnames[r0], imm12, regnames[r1]);
    }

    // Store instructions.

    // sb r0, imm12(r1)
    std::string Sb(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        return std::format("sb {}, {}({})", regnames[r0], imm12, regnames[r1]);
    }

    // sh r0, imm12(r1)
    std::string Sh(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        return std::format("sh {}, {}({})", regnames[r0], imm12, regnames[r1]);
    }

    // sw r0, imm12(r1)
    std::string Sw(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        return std::format("sw {}, {}({})", regnames[r0], imm12, regnames[r1]);
    }

    // Memory ordering instructions.

    std::string Fence()
    {
        return "fence";
    }

    // Subroutine call instructions.

    // jalr r0, offs12(r1)
    std::string Jalr(uint32_t r0, int32_t offs12, uint32_t r1)
    {
        if (r0 == zero && r1 == ra && offs12 == 0)
        {
            return "ret";
        }
        else
        {
            return std::format("jalr {}, {}({})", regnames[r0], offs12, regnames[r1]);
        }
    }

    // jal r0, offs20
    std::string Jal(uint32_t r0, int32_t offs20)
    {
        if (r0 == ra)
        {
            return std::format("jal {}", offs20);
        }
        else
        {
            return std::format("jal {}, {}", regnames[r0], offs20);
        }
    }

    // Miscellaneous instructions.

    // lui r0, uimm20
    std::string Lui(uint32_t r0, uint32_t uimm20)
    {
        return std::format("lui {}, {}", regnames[r0], uimm20);
    }

    // auipc r0, uimm20
    std::string Auipc(uint32_t r0, uint32_t uimm20)
    {
        return std::format("auipc {}, {}", regnames[r0], uimm20);
    }

    // Owl-2820 only instructions.

    // j offs20
    std::string J(int32_t offs20)
    {
        return std::format("j {}", offs20);
    }

    // call offs20
    std::string Call(int32_t offs20)
    {
        return std::format("call {}", offs20);
    }

    // ret
    std::string Ret()
    {
        return "ret";
    }

    // li r0, imm12
    std::string Li(uint32_t r0, int32_t imm12)
    {
        return std::format("li {}, {}", regnames[r0], imm12);
    }

    // mv r0, r1
    std::string Mv(uint32_t r0, uint32_t r1)
    {
        return std::format("mv {}, {}", regnames[r0], regnames[r1]);
    }

    // illegal
    std::string Illegal(uint32_t ins)
    {
        return std::format("illegal {:08x}", ins);
    }
};

static_assert(InstructionHandler<Disassembler>);
