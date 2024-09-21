#pragma once

#include "endian.h"
#include "instruction_handler.h"
#include "memory.h"

#include <cstdint>
#include <iostream>
#include <span>

// Symbolic register names.
enum
{
    zero,
    ra,
    sp,
    gp,
    tp,
    t0,
    t1,
    t2,
    s0,
    s1,
    a0,
    a1,
    a2,
    a3,
    a4,
    a5,
    a6,
    a7,
    s2,
    s3,
    s4,
    s5,
    s6,
    s7,
    s8,
    s9,
    s10,
    s11,
    t3,
    t4,
    t5,
    t6
};

enum Syscall
{
    Exit,
    PrintFib
};

class OwlCpu
{
    uint32_t pc = 0;             // The program counter.
    uint32_t nextPc = 0;         // The address of the next instruction.
    uint32_t x[32] = {};         // The integer registers.
    bool done = false;           // Are we there yet?
    std::span<uint32_t> code;    // Non-owning.
    std::span<std::byte> memory; // Non-owning.

public:
    using Item = void;

    OwlCpu(std::span<std::uint32_t> image) : code{image}, memory{std::as_writable_bytes(image)}
    {
        // Set the stack pointer to the end of memory.
        x[sp] = uint32_t(memory.size());
    }

    bool Done() const
    {
        return done;
    }

    uint32_t Fetch()
    {
        constexpr uint32_t wordSize = sizeof(uint32_t);
        pc = nextPc;
        nextPc += wordSize;
        return AsLE(code[pc / wordSize]);
    }

    // System instructions.

    void Ecall()
    {
        const auto syscall = Syscall(x[a7]);
        switch (syscall)
        {
        case Syscall::Exit:
            // TODO(lbavm-011): Turn this off when benchmarking.
            std::cout << "Exiting with status " << x[a0] << '\n';
            done = true;
            break;

        case Syscall::PrintFib:
            // TODO(lbavm-011): Turn this off when benchmarking.
            std::cout << "fib(" << x[a0] << ") = " << x[a1] << '\n';
            break;
        }
    }

    void Ebreak()
    {
        done = true;
    }

    // Register-register instructions.

    // add r0, r1, r2
    void Add(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // r0 <- r1 + r2
        x[r0] = x[r1] + x[r2];
        x[0] = 0; // Ensure x0 is always zero.
    }

    // sub r0, r1, r2
    void Sub(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // r0 <- r1 + r2
        x[r0] = x[r1] - x[r2];
        x[0] = 0; // Ensure x0 is always zero.
    }

    // sll r0, r1, r2
    void Sll(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // r0 <- r1 << (r2 % 32)
        x[r0] = x[r1] << (x[r2] % 32);
        x[0] = 0; // Ensure x0 is always zero.
    }

    // slt r0, r1, r2
    void Slt(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // r0 <- (r1 < r2) ? 1 : 0 (signed integer comparison)
        const int32_t sr1 = static_cast<int32_t>(x[r1]);
        const int32_t sr2 = static_cast<int32_t>(x[r2]);
        x[r0] = sr1 < sr2 ? 1 : 0;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // sltu r0, r1, r2
    void Sltu(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // r0 <- (r1 < r2) ? 1 : 0 (unsigned integer comparison)
        x[r0] = x[r1] < x[r2] ? 1 : 0;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // xor r0, r1, r2
    void Xor(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // r0 <- r1 ^ r2
        x[r0] = x[r1] ^ x[r2];
        x[0] = 0; // Ensure x0 is always zero.
    }

    // srl r0, r1, r2
    void Srl(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // r0 <- r1 >> (r2 % 32) (shift right logical)
        x[r0] = x[r1] >> (x[r2] % 32);
        x[0] = 0; // Ensure x0 is always zero.
    }

    // sra r0, r1, r2
    void Sra(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // r0 <- r1 >> (r2 % 32) (shift right arithmetic)
        const int32_t sr1 = static_cast<int32_t>(x[r1]);
        const int32_t shift = static_cast<int32_t>(x[r2]) % 32;
        x[r0] = static_cast<uint32_t>(sr1 >> shift);
        x[0] = 0; // Ensure x0 is always zero.
    }

    // or r0, r1, r2
    void Or(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // r0 <- r1 | r2
        x[r0] = x[r1] | x[r2];
        x[0] = 0; // Ensure x0 is always zero.
    }

    // and r0, r1, r2
    void And(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // r0 <- r1 & r2
        x[r0] = x[r1] & x[r2];
        x[0] = 0; // Ensure x0 is always zero.
    }

    // Immediate shift instructions.

    // slli r0, r1, shift
    void Slli(uint32_t r0, uint32_t r1, uint32_t shift)
    {
        // r0 <- r1 << shift
        x[r0] = x[r1] << shift;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // srli r0, r1, shift
    void Srli(uint32_t r0, uint32_t r1, uint32_t shift)
    {
        // r0 <- r1 >> shift (shift right logical)
        const int32_t sr1 = static_cast<int32_t>(x[r1]);
        const int32_t numBits = shift;
        x[r0] = static_cast<uint32_t>(sr1 >> numBits);
        x[0] = 0; // Ensure x0 is always zero.
    }

    // srai r0, r1, shift
    void Srai(uint32_t r0, uint32_t r1, uint32_t shift)
    {
        // r0 <- r1 >> shift (shift right logical)
        x[r0] = x[r1] >> shift;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // Branch instructions.

    // beq r0, r1, offs12
    void Beq(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        // pc <- pc + ((r0 == r1) ? offs12 : 4
        if (x[r0] == x[r1])
        {
            nextPc = pc + offs12; // Take the branch.
        }
    }

    // bne r0, r1, offs12
    void Bne(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        // pc <- pc + ((r0 != r1) ? offs12 : 4)
        if (x[r0] != x[r1])
        {
            nextPc = pc + offs12; // Take the branch.
        }
    }

    // blt r0, r1, offs12
    void Blt(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        // pc <- pc + ((r0 < r1) ? offs12 : 4) (signed integer comparison)
        int32_t sr0 = static_cast<int32_t>(x[r0]);
        int32_t sr1 = static_cast<int32_t>(x[r1]);
        if (sr0 < sr1)
        {
            nextPc = pc + offs12; // Take the branch.
        }
    }

    // bge r0, r1, offs12
    void Bge(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        // pc <- pc + ((r0 >= r1) ? offs12 : 4) (signed integer comparison)
        int32_t sr0 = static_cast<int32_t>(x[r0]);
        int32_t sr1 = static_cast<int32_t>(x[r1]);
        if (sr0 >= sr1)
        {
            nextPc = pc + offs12; // Take the branch.
        }
    }

    // bltu r0, r1, offs12
    void Bltu(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        // pc <- pc + ((r0 < r1) ? offs12 : 4) (unsigned integer comparison)
        if (x[r0] < x[r1])
        {
            nextPc = pc + offs12; // Take the branch.
        }
    }

    // bgeu r0, r1, offs12
    void Bgeu(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        // pc <- pc + ((r0 >= r1) ? offs12 : 4) (unsigned integer comparison)
        if (x[r0] >= x[r1])
        {
            nextPc = pc + offs12; // Take the branch.
        }
    }

    // Register-immediate instructions.

    // addi r0, r1, imm12
    void Addi(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        // r0 <- r1 + imm12
        x[r0] = x[r1] + imm12;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // slti r0, r1, imm12
    void Slti(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        // r0 <- (r1 < imm12) ? 1 : 0 (signed integer comparison)
        const int32_t sr1 = static_cast<int32_t>(x[r1]);
        const int32_t simm12 = static_cast<int32_t>(x[imm12]);
        x[r0] = sr1 < simm12 ? 1 : 0;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // sltiu r0, r1, imm12
    void Sltiu(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        // r0 <- (r1 < imm12) ? 1 : 0 (unsigned integer comparison)
        x[r0] = x[r1] < uint32_t(imm12) ? 1 : 0;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // xori r0, r1, imm12
    void Xori(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        // r0 <- r1 ^ imm12
        x[r0] = x[r1] ^ imm12;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // ori r0, r1, imm12
    void Ori(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        // r0 <- r1 | imm12
        x[r0] = x[r1] | imm12;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // andi r0, r1, imm12
    void Andi(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        // r0 <- r1 & imm12
        x[r0] = x[r1] & imm12;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // Load instructions.

    // lb r0, imm12(r1)
    void Lb(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // r0 <- sext(memory8(r1 + imm12))
        const uint32_t addr = x[r1] + imm12;
        const int32_t signExtendedByte = static_cast<int8_t>(Read8(memory, addr));
        x[r0] = signExtendedByte;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // lbu r0, imm12(r1)
    void Lbu(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // r0 <- zext(memory(r1 + imm12))
        const uint32_t addr = x[r1] + imm12;
        const uint32_t zeroExtendedByte = static_cast<uint8_t>(Read8(memory, addr));
        x[r0] = zeroExtendedByte;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // lh r0, imm12(r1)
    void Lh(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // r0 <- sext(memory16(r1 + imm12))
        const uint32_t addr = x[r1] + imm12;
        const int32_t signExtendedHalfWord = static_cast<int16_t>(Read16(memory, addr));
        x[r0] = signExtendedHalfWord;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // lhu r0, imm12(r1)
    void Lhu(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // r0 <- zext(memory16(r1 + imm12))
        const uint32_t addr = x[r1] + imm12;
        const uint32_t zeroExtendedHalfWord = static_cast<uint16_t>(Read16(memory, addr));
        x[r0] = zeroExtendedHalfWord;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // lw r0, imm12(r1)
    void Lw(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // r0 <- memory32(r1 + imm12)
        const uint32_t addr = x[r1] + imm12;
        x[r0] = Read32(memory, addr);
        x[0] = 0; // Ensure x0 is always zero.
    }

    // Store instructions.

    // sb r0, imm12(r1)
    void Sb(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // memory8(r1 + imm12) <- r0[7:0]
        const uint32_t addr = x[r1] + imm12;
        const std::byte byte = static_cast<std::byte>(x[r0]);
        Write8(memory, addr, byte);
    }

    // sh r0, imm12(r1)
    void Sh(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // memory16(r1 + imm12) <- r0[15:0]
        const uint32_t addr = x[r1] + imm12;
        const uint16_t halfWord = static_cast<uint16_t>(x[r0]);
        Write16(memory, addr, halfWord);
    }

    // sw r0, imm12(r1)
    void Sw(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // memory32(r1 + imm12) <- r0
        const uint32_t addr = x[r1] + imm12;
        const uint32_t word = x[r0];
        Write32(memory, addr, word);
    }

    // Memory ordering instructions.

    void Fence()
    {
        // fence
    }

    // Subroutine call instructions.

    // jalr r0, offs12(r1)
    void Jalr(uint32_t r0, int32_t offs12, uint32_t r1)
    {
        // r0 <- pc + 4, pc <- r1 + offs12
        const int32_t r1Before = x[r1]; // r0 and r1 may be the same register.
        x[r0] = nextPc;
        nextPc = r1Before + offs12;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // jal r0, offs20
    void Jal(uint32_t r0, int32_t offs20)
    {
        // r0 <- pc + 4, pc <- pc + offs20
        x[r0] = nextPc;
        nextPc = pc + offs20;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // Miscellaneous instructions.

    // lui r0, uimm20
    void Lui(uint32_t r0, uint32_t uimm20)
    {
        // r0 <- uimm20 & 0xfffff000
        x[r0] = uimm20;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // auipc r0, uimm20
    void Auipc(uint32_t r0, uint32_t uimm20)
    {
        // r0 <- pc + (uimm20 & 0xfffff000)
        x[r0] = pc + uimm20;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // Owl-2820 only instructions.

    // j offs20
    void J(int32_t offs20)
    {
        // pc <- pc + offs20
        nextPc = pc + offs20; // Branch.
    }

    // call offs20
    void Call(int32_t offs20)
    {
        // ra <- pc + 4, pc <- pc + offs20
        x[ra] = nextPc;       // Save the return address in ra.
        nextPc = pc + offs20; // Make the call.
    }

    // ret
    void Ret()
    {
        // pc <- ra
        nextPc = x[ra]; // Return to ra.
    }

    // li r0, imm12
    void Li(uint32_t r0, int32_t imm12)
    {
        // r0 <- imm12
        x[r0] = imm12;
        x[0] = 0; // Ensure x0 is always zero.
    }

    // mv r0, r1
    void Mv(uint32_t r0, uint32_t r1)
    {
        // r0 <- r1
        x[r0] = x[r1];
        x[0] = 0; // Ensure x0 is always zero.
    }

    void Illegal([[maybe_unused]] uint32_t ins)
    {
        // If we don't recognise the opcode then by default we have an illegal instruction.
        done = true;
    }
};

static_assert(InstructionHandler<OwlCpu>);
