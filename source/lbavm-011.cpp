#include "assembler.h"
#include "endian.h"
#include "opcodes.h"

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

using Memory = std::span<std::byte>;

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

std::byte Read8(const Memory memory, uint32_t addr)
{
    return memory[addr];
}

std::uint16_t Read16(const Memory memory, uint32_t addr)
{
    // Owl-2820 is permissive about unaligned memory accesses. This may not be the case for
    // the host platform, so we do the equivalent of a memcpy from the VM's memory before
    // trying to interpret the value. Most compilers will detect what we're doing and
    // optimize it away.
    uint16_t v;
    std::ranges::copy_n(memory.data() + addr, sizeof(v), reinterpret_cast<std::byte*>(&v));

    // Owl-2820 is little-endian, so swap the byte order if necessary.
    return AsLE(v);
}

uint32_t Read32(const Memory memory, uint32_t addr)
{
    // Owl-2820 is permissive about unaligned memory accesses. This may not be the case for
    // the host platform, so we do the equivalent of a memcpy from the VM's memory before
    // trying to interpret the value. Most compilers will detect what we're doing and
    // optimize it away.
    uint32_t v;
    std::ranges::copy_n(memory.data() + addr, sizeof(uint32_t), reinterpret_cast<std::byte*>(&v));

    // Owl-2820 is little-endian, so swap the byte order if necessary.
    return AsLE(v);
}

void Write8(Memory memory, uint32_t addr, std::byte byte)
{
    memory[addr] = byte;
}

void Write16(Memory memory, uint32_t addr, uint16_t halfWord)
{
    // Owl-2820 is little-endian, so swap the byte order if necessary.
    const uint16_t v = AsLE(halfWord);

    // Owl-2820 is permissive about unaligned memory accesses. This may not be the case for
    // the host platform, so we do the equivalent of a memcpy when writing the value to the
    // VM's memory. Most compilers will detect what we're doing and optimize it away.
    std::ranges::copy_n(reinterpret_cast<const std::byte*>(&v), sizeof(v), memory.data() + addr);
}

void Write32(Memory memory, uint32_t addr, uint32_t word)
{
    // Owl-2820 is little-endian, so swap the byte order if necessary.
    const uint32_t v = AsLE(word);

    // Owl-2820 is permissive about unaligned memory accesses. This may not be the case for
    // the host platform, so we do the equivalent of a memcpy when writing the value to the
    // VM's memory. Most compilers will detect what we're doing and optimize it away.
    std::ranges::copy_n(reinterpret_cast<const std::byte*>(&v), sizeof(uint32_t),
                        memory.data() + addr);
}

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
            std::cout << "Exiting with status " << x[a0] << '\n';
            done = true;
            break;

        case Syscall::PrintFib:
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
