#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

// This code works on little-endian and big-endian platforms only.
static_assert(std::endian::native == std::endian::little
              || std::endian::native == std::endian::big);

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

// Opcodes.
enum class Opcode : uint32_t
{
    Illegal = 0,
    // System instructions.
    Ecall,
    Ebreak,
    // Register-register instructions.
    Add,
    Sub,
    Sll,
    Slt,
    Sltu,
    Xor,
    Srl,
    Sra,
    Or,
    And,
    // Immediate shift instructions.
    Slli,
    Srli,
    Srai,
    // Branch instructions.
    Beq,
    Bne,
    Blt,
    Bge,
    Bltu,
    Bgeu,
    // Register-immediate instructions.
    Addi,
    Slti,
    Sltiu,
    Xori,
    Ori,
    Andi,
    // Load instructions.
    Lb,
    Lbu,
    Lh,
    Lhu,
    Lw,
    // Store instructions.
    Sb,
    Sh,
    Sw,
    // Memory ordering instructions.
    Fence,
    // Subroutine call instructions.
    Jalr,
    Jal,
    // Miscellaneous instructions.
    Lui,
    Auipc,
    // Owl-2820 only instructions.
    J,
    Call,
    Ret,
    Li,
    Mv,
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

uint16_t AsLE(uint16_t halfWord)
{
    if constexpr (std::endian::native == std::endian::little)
    {
        // Nothing to do. We're on a little-endian platform, and Owl-2820 is little-endian.
        return halfWord;
    }

    if constexpr (std::endian::native == std::endian::big)
    {
        // We're on a big-endian platform so reverse the byte order as Owl-2820 is little-endian.
        // The code may look like it takes lots of instructions, but MSVC, gcc,and clang are clever
        // enough to figure out what is going on and will generate the equivalent of `bswap` (x86)
        // or `rev` (ARM).
        //
        // std::rotr(halfWord & 00ff, 8)
        //      1234 & 00ff                     == 0034
        //      0034 rotr 8                     == 3400
        //
        // std::rotl(halfWord, 8) & 00ff
        //      1234 rotl 8                     == 3412
        //      3412 & 00ff                     == 0012
        //
        // 3400 | 0012                          == 3412
        //
        return std::rotr(halfWord & 0x00ffu, 8) | (std::rotl(halfWord, 8) & 0x00ffu);
    }
}

uint32_t AsLE(uint32_t word)
{
    if constexpr (std::endian::native == std::endian::little)
    {
        // Nothing to do. We're on a little-endian platform, and Owl-2820 is little-endian.
        return word;
    }

    if constexpr (std::endian::native == std::endian::big)
    {
        // We're on a big-endian platform so reverse the byte order as Owl-2820 is little-endian.
        // The code may look like it takes lots of instructions, but MSVC, gcc,and clang are clever
        // enough to figure out what is going on and will generate the equivalent of `bswap` (x86)
        // or `rev` (ARM).
        //
        // std::rotr(word & 00ff00ff, 8)
        //      12345678 & 00ff00ff             == 00340078
        //      00340078 rotr 8                 == 78004500
        //
        // std::rotl(word, 8) & 00ff00ff
        //      12345678 rotl 8                 == 34567812
        //      34567812 & 00ff00ff             == 00560012
        //
        // 78004500 | 00560012                  == 78563412
        //
        return std::rotr(word & 0x00ff00ff, 8) | (std::rotl(word, 8) & 0x00ff00ff);
    }
}

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

template<typename T>
concept OwlHandler =
        requires(T t, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t shift, int32_t offs12,
                 int32_t imm12, int32_t offs20, uint32_t uimm20, uint32_t ins) {
            //
            { t.Ecall() } -> std::same_as<void>;
            { t.Ebreak() } -> std::same_as<void>;
            //
            { t.Add(r0, r1, r2) } -> std::same_as<void>;
            { t.Sub(r0, r1, r2) } -> std::same_as<void>;
            { t.Sll(r0, r1, r2) } -> std::same_as<void>;
            { t.Slt(r0, r1, r2) } -> std::same_as<void>;
            { t.Sltu(r0, r1, r2) } -> std::same_as<void>;
            { t.Xor(r0, r1, r2) } -> std::same_as<void>;
            { t.Srl(r0, r1, r2) } -> std::same_as<void>;
            { t.Sra(r0, r1, r2) } -> std::same_as<void>;
            { t.Or(r0, r1, r2) } -> std::same_as<void>;
            { t.And(r0, r1, r2) } -> std::same_as<void>;
            //
            { t.Slli(r0, r1, shift) } -> std::same_as<void>;
            { t.Srli(r0, r1, shift) } -> std::same_as<void>;
            { t.Srai(r0, r1, shift) } -> std::same_as<void>;
            //
            { t.Beq(r0, r1, offs12) } -> std::same_as<void>;
            { t.Bne(r0, r1, offs12) } -> std::same_as<void>;
            { t.Blt(r0, r1, offs12) } -> std::same_as<void>;
            { t.Bge(r0, r1, offs12) } -> std::same_as<void>;
            { t.Bltu(r0, r1, offs12) } -> std::same_as<void>;
            { t.Bgeu(r0, r1, offs12) } -> std::same_as<void>;
            //
            { t.Addi(r0, r1, imm12) } -> std::same_as<void>;
            { t.Slti(r0, r1, imm12) } -> std::same_as<void>;
            { t.Sltiu(r0, r1, imm12) } -> std::same_as<void>;
            { t.Xori(r0, r1, imm12) } -> std::same_as<void>;
            { t.Ori(r0, r1, imm12) } -> std::same_as<void>;
            { t.Andi(r0, r1, imm12) } -> std::same_as<void>;
            //
            { t.Lb(r0, imm12, r1) } -> std::same_as<void>;
            { t.Lbu(r0, imm12, r1) } -> std::same_as<void>;
            { t.Lh(r0, imm12, r1) } -> std::same_as<void>;
            { t.Lhu(r0, imm12, r1) } -> std::same_as<void>;
            { t.Lw(r0, imm12, r1) } -> std::same_as<void>;
            //
            { t.Sb(r0, imm12, r1) } -> std::same_as<void>;
            { t.Sh(r0, imm12, r1) } -> std::same_as<void>;
            { t.Sw(r0, imm12, r1) } -> std::same_as<void>;
            //
            { t.Fence() } -> std::same_as<void>;
            //
            { t.Jalr(r0, offs12, r1) } -> std::same_as<void>;
            { t.Jal(r0, offs20) } -> std::same_as<void>;
            //
            { t.Lui(r0, uimm20) } -> std::same_as<void>;
            { t.Auipc(r0, uimm20) } -> std::same_as<void>;
            //
            { t.J(offs20) } -> std::same_as<void>;
            { t.Call(offs20) } -> std::same_as<void>;
            { t.Ret() } -> std::same_as<void>;
            { t.Li(r0, imm12) } -> std::same_as<void>;
            { t.Mv(r0, r1) } -> std::same_as<void>;
            //
            { t.Illegal(ins) } -> std::same_as<void>;
        };

class OwlCpu
{
private:
    uint32_t pc = 0;             // The program counter.
    uint32_t nextPc = 0;         // The address of the next instruction.
    uint32_t x[32] = {};         // The integer registers.
    bool done = false;           // Are we there yet?
    std::span<std::byte> memory; // TODO: not at all safe!!!

public:
    void Run(std::span<uint32_t> image);

    // System instructions.

    void Ecall()
    {
        const auto syscall = Syscall(x[a7]);
        switch (syscall)
        {
        case Syscall::Exit:
            std::cout << std::format("Exiting with status {}\n", x[a0]);
            done = true;
            break;

        case Syscall::PrintFib:
            std::cout << std::format("fib({}) = {}\n", x[a0], x[a1]);
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

    // Illegal.

    void Illegal([[maybe_unused]] uint32_t ins)
    {
        // If we don't recognise the opcode then by default we have an illegal instruction.
        done = true;
    }
};

void DispatchOwl(OwlHandler auto& cpu, uint32_t ins);

void OwlCpu::Run(std::span<uint32_t> image)
{
    // Get a read-only, word addressable view of the image for fetching instructions.
    const auto code = image;

    // Get a byte-addressable view of the image for memory accesses.
    memory = std::as_writable_bytes(image);

    // Set pc and nextPc to their initial values.
    pc = 0;     // The program counter.
    nextPc = 0; // The address of the next instruction.

    // Set all the integer registers to zero.
    x[32] = {}; // The integer registers.

    // Set the stack pointer to the end of memory.
    x[sp] = uint32_t(memory.size());

    constexpr uint32_t wordSize = sizeof(uint32_t);

    done = false;

    while (!done)
    {
        // Fetch a 32-bit word from the address pointed to by the program counter.
        pc = nextPc;
        nextPc += wordSize;
        const uint32_t ins = AsLE(code[pc / wordSize]);
        DispatchOwl(*this, ins);
    }
}

static_assert(OwlHandler<OwlCpu>);

void DispatchOwl(OwlHandler auto& cpu, uint32_t ins)
{
    using namespace decode;

    // Decode the word to extract the opcode.
    const Opcode opcode = Opcode(ins & 0x7f);

    // Dispatch it and execute it.
    switch (opcode)
    {
        // clang-format off
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
        // clang-format on
    }
}

namespace encode
{
    uint32_t opc(Opcode opcode)
    {
        return uint32_t(opcode);
    }

    uint32_t r0(uint32_t r)
    {
        return (r & 0x1f) << 7;
    }

    uint32_t r1(uint32_t r)
    {
        return (r & 0x1f) << 12;
    }

    uint32_t r2(uint32_t r)
    {
        return (r & 0x1f) << 17;
    }

    uint32_t shift(uint32_t uimm5)
    {
        return (uimm5 & 0x1f) << 17;
    }

    uint32_t imm12(int32_t imm12)
    {
        return static_cast<uint32_t>(imm12 << 20);
    }

    uint32_t offs12(int32_t offs12)
    {
        // Assumes that offs12 is pre-multiplied to a byte offset.
        return static_cast<uint32_t>(offs12 << 19) & 0xfff00000;
    }

    uint32_t offs20(int32_t offs20)
    {
        // Assumes that offs20 is pre-multiplied to a byte offset.
        return static_cast<uint32_t>(offs20 << 11) & 0xfffff000;
    }

    uint32_t uimm20(uint32_t uimm20)
    {
        return (uimm20 << 12) & 0xfffff000;
    }
} // namespace encode

class Label
{
public:
    using Id = size_t;

    explicit Label(Id id) : id_{id}
    {
    }

    Id GetId() const
    {
        return id_;
    }

private:
    Id id_;
};

class Assembler
{
    static constexpr uint32_t badAddress = static_cast<uint32_t>(-1);

    enum class FixupType
    {
        offs12,
        offs20,
        hi20,
        lo12
    };

    struct Fixup
    {
        uint32_t target; // The address that contains the data to be fixed up.
        FixupType type;  // The type of the data that needs to be fixed up.
    };

    std::vector<uint32_t> code_;
    uint32_t current_{};
    std::vector<uint32_t> labels_;
    std::multimap<Label::Id, Fixup> fixups_;

    uint32_t Current() const
    {
        return current_;
    }

    std::optional<uint32_t> AddressOf(Label label)
    {
        if (auto result = labels_[label.GetId()]; result != badAddress)
        {
            return result;
        }
        return {};
    }

    template<FixupType type>
    void ResolveFixup(uint32_t addr, int32_t offset)
    {
        auto existing = code_[addr / 4];
        if constexpr (type == FixupType::offs12)
        {
            existing = (existing & 0x000fffff) | encode::offs12(offset);
        }
        else if constexpr (type == FixupType::offs20)
        {
            existing = (existing & 0x00000fff) | encode::offs20(offset);
        }
        else if constexpr (type == FixupType::hi20)
        {
            // Assumes that the offset is pre-encoded as a uimm20.
            existing = (existing & 0x00000fff) | offset;
        }
        else if constexpr (type == FixupType::lo12)
        {
            existing = (existing & 0x000fffff) | encode::imm12(offset);
        }
        code_[addr / 4] = existing;
    }

    template<FixupType type>
    void AddFixup(Label label)
    {
        fixups_.emplace(label.GetId(), Fixup{.target = Current(), .type = type});
    }

public:
    void BindLabel(Label label)
    {
        const auto id = label.GetId();
        const auto address = Current();
        labels_[id] = address;

        // Find all the fixups for this label id.
        if (const auto fixupsForId = fixups_.equal_range(id); fixupsForId.first != fixups_.end())
        {
            // Resolve the fixups.
            for (auto [_, fixup] : std::ranges::subrange(fixupsForId.first, fixupsForId.second))
            {
                if (fixup.type == FixupType::offs12)
                {
                    const auto offset = address - fixup.target;
                    ResolveFixup<FixupType::offs12>(fixup.target, offset);
                }
                else if (fixup.type == FixupType::offs20)
                {
                    const auto offset = address - fixup.target;
                    ResolveFixup<FixupType::offs20>(fixup.target, offset);
                }
                else if (fixup.type == FixupType::hi20)
                {
                    const auto upper20 = (address & 0xfffff000);
                    ResolveFixup<FixupType::hi20>(fixup.target, upper20);
                }
                else if (fixup.type == FixupType::lo12)
                {
                    const auto lower12 = (address & 0x00000fff);
                    ResolveFixup<FixupType::lo12>(fixup.target, lower12);
                }
            }

            // We don't need these fixups any more.
            fixups_.erase(id);
        }
    }

    Label MakeLabel()
    {
        auto labelId = labels_.size();
        labels_.push_back(badAddress);
        return Label(labelId);
    }

    const std::vector<uint32_t>& Code() const
    {
        if (fixups_.empty())
        {
            return code_;
        }
        throw std::runtime_error("There are unbound labels.");
    }

    void Emit(uint32_t u)
    {
        code_.push_back(AsLE(u));
        current_ += 4; // 4 bytes per instruction.
    }

    // System instructions.

    void Ecall()
    {
        Emit(encode::opc(Opcode::Ecall));
    }

    void Ebreak()
    {
        Emit(encode::opc(Opcode::Ebreak));
    }

    // Register-register instructions.

    // add r0, r1, r2
    void Add(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(Opcode::Add) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    // sub r0, r1, r2
    void Sub(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(Opcode::Sub) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    // sll r0, r1, r2
    void Sll(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(Opcode::Sll) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    // slt r0, r1, r2
    void Slt(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(Opcode::Slt) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    // sltu r0, r1, r2
    void Sltu(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(Opcode::Sltu) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    // xor r0, r1, r2
    void Xor(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(Opcode::Xor) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    // srl r0, r1, r2
    void Srl(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(Opcode::Srl) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    // sra r0, r1, r2
    void Sra(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(Opcode::Sra) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    // or r0, r1, r2
    void Or(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(Opcode::Or) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    // and r0, r1, r2
    void And(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(Opcode::And) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    // Immediate shift instructions.

    // slli r0, r1, shift
    void Slli(uint32_t r0, uint32_t r1, uint32_t shift)
    {
        Emit(encode::opc(Opcode::Slli) | encode::r0(r0) | encode::r1(r1) | encode::shift(shift));
    }

    // srli r0, r1, shift
    void Srli(uint32_t r0, uint32_t r1, uint32_t shift)
    {
        Emit(encode::opc(Opcode::Srli) | encode::r0(r0) | encode::r1(r1) | encode::shift(shift));
    }

    // srai r0, r1, shift
    void Srai(uint32_t r0, uint32_t r1, uint32_t shift)
    {
        Emit(encode::opc(Opcode::Srai) | encode::r0(r0) | encode::r1(r1) | encode::shift(shift));
    }

    // Branch instructions.

    template<Opcode opcode>
    void Branch(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        Emit(encode::opc(opcode) | encode::r0(r0) | encode::r1(r1) | encode::offs12(offs12));
    }

    template<Opcode opcode>
    void Branch(uint32_t r0, uint32_t r1, Label label)
    {
        if (const auto addr = AddressOf(label); addr.has_value())
        {
            Branch<opcode>(r0, r1, *addr - Current());
        }
        else
        {
            AddFixup<FixupType::offs12>(label);
            Branch<opcode>(r0, r1, 0);
        }
    }

    // beq r0, r1, offs12
    void Beq(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        Branch<Opcode::Beq>(r0, r1, offs12);
    }

    // beq r0, r1, label
    void Beq(uint32_t r0, uint32_t r1, Label label)
    {
        Branch<Opcode::Beq>(r0, r1, label);
    }

    // bne r0, r1, offs12
    void Bne(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        Branch<Opcode::Bne>(r0, r1, offs12);
    }

    // bne r0, r1, label
    void Bne(uint32_t r0, uint32_t r1, Label label)
    {
        Branch<Opcode::Bne>(r0, r1, label);
    }

    // blt r0, r1, offs12
    void Blt(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        Branch<Opcode::Blt>(r0, r1, offs12);
    }

    // blt r0, r1, label
    void Blt(uint32_t r0, uint32_t r1, Label label)
    {
        Branch<Opcode::Blt>(r0, r1, label);
    }

    // bge r0, r1, offs12
    void Bge(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        Branch<Opcode::Bge>(r0, r1, offs12);
    }

    // bge r0, r1, label
    void Bge(uint32_t r0, uint32_t r1, Label label)
    {
        Branch<Opcode::Bge>(r0, r1, label);
    }

    // bltu r0, r1, offs12
    void Bltu(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        Branch<Opcode::Bltu>(r0, r1, offs12);
    }

    // bltu r0, r1, label
    void Bltu(uint32_t r0, uint32_t r1, Label label)
    {
        Branch<Opcode::Bltu>(r0, r1, label);
    }

    // bgeu r0, r1, offs12
    void Bgeu(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        Branch<Opcode::Bgeu>(r0, r1, offs12);
    }

    // bgeu r0, r1, label
    void Bgeu(uint32_t r0, uint32_t r1, Label label)
    {
        Branch<Opcode::Bgeu>(r0, r1, label);
    }

    // Register-immediate instructions.

    // addi r0, r1, imm12
    void Addi(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        Emit(encode::opc(Opcode::Addi) | encode::r0(r0) | encode::r1(r1) | encode::imm12(imm12));
    }

    // slti r0, r1, imm12
    void Slti(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        Emit(encode::opc(Opcode::Slti) | encode::r0(r0) | encode::r1(r1) | encode::imm12(imm12));
    }

    // sltiu r0, r1, imm12
    void Sltiu(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        Emit(encode::opc(Opcode::Sltiu) | encode::r0(r0) | encode::r1(r1) | encode::imm12(imm12));
    }

    // xori r0, r1, imm12
    void Xori(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        Emit(encode::opc(Opcode::Xori) | encode::r0(r0) | encode::r1(r1) | encode::imm12(imm12));
    }

    // ori r0, r1, imm12
    void Ori(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        Emit(encode::opc(Opcode::Ori) | encode::r0(r0) | encode::r1(r1) | encode::imm12(imm12));
    }

    // andi r0, r1, imm12
    void Andi(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        Emit(encode::opc(Opcode::Andi) | encode::r0(r0) | encode::r1(r1) | encode::imm12(imm12));
    }

    // Load instructions.

    // lb r0, imm12(r1)
    void Lb(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        Emit(encode::opc(Opcode::Lb) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    // lbu r0, imm12(r1)
    void Lbu(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        Emit(encode::opc(Opcode::Lbu) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    // lh r0, imm12(r1)
    void Lh(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        Emit(encode::opc(Opcode::Lh) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    // lhu r0, imm12(r1)
    void Lhu(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        Emit(encode::opc(Opcode::Lhu) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    // lw r0, imm12(r1)
    void Lw(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        Emit(encode::opc(Opcode::Lw) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    // Store instructions.

    // sb r0, imm12(r1)
    void Sb(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        Emit(encode::opc(Opcode::Sb) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    // sh r0, imm12(r1)
    void Sh(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        Emit(encode::opc(Opcode::Sh) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    // sw r0, imm12(r1)
    void Sw(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        Emit(encode::opc(Opcode::Sw) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    // Memory ordering instructions.

    void Fence()
    {
        // fence
        Emit(encode::opc(Opcode::Fence));
    }

    // Subroutine call instructions.

    // jalr r0, offs12(r1)
    void Jalr(uint32_t r0, int32_t offs12, uint32_t r1)
    {
        Emit(encode::opc(Opcode::Jalr) | encode::offs12(offs12) | encode::r1(r1) | encode::r0(r0));
    }

    // jal r0, offs20
    void Jal(uint32_t r0, int32_t offs20)
    {
        Emit(encode::opc(Opcode::Jal) | encode::offs20(offs20) | encode::r0(r0));
    }

    // Miscellaneous instructions.

    // lui r0, uimm20
    void Lui(uint32_t r0, uint32_t uimm20)
    {
        Emit(encode::opc(Opcode::Lui) | encode::r0(r0) | encode::uimm20(uimm20));
    }

    // auipc r0, uimm20
    void Auipc(uint32_t r0, uint32_t uimm20)
    {
        Emit(encode::opc(Opcode::Auipc) | encode::r0(r0) | encode::uimm20(uimm20));
    }

    // Owl-2820 only instructions.

    template<Opcode opcode>
    void Jump(int32_t offs20)
    {
        Emit(encode::opc(opcode) | encode::offs20(offs20));
    }

    template<Opcode opcode>
    void Jump(Label label)
    {
        if (const auto addr = AddressOf(label); addr.has_value())
        {
            Jump<opcode>(*addr - Current());
        }
        else
        {
            AddFixup<FixupType::offs20>(label);
            Jump<opcode>(0);
        }
    }

    // j offs20
    void J(int32_t offs20)
    {
        Jump<Opcode::J>(offs20);
    }

    // j label
    void J(Label label)
    {
        Jump<Opcode::J>(label);
    }

    // call offs20
    void Call(int32_t offs20)
    {
        Jump<Opcode::Call>(offs20);
    }

    // call label
    void Call(Label label)
    {
        Jump<Opcode::Call>(label);
    }

    // ret
    void Ret()
    {
        Emit(encode::opc(Opcode::Ret));
    }

    // li r0, imm12
    void Li(uint32_t r0, int32_t imm12)
    {
        Emit(encode::opc(Opcode::Li) | encode::r0(r0) | encode::imm12(imm12));
    }

    // mv r0, r1
    void Mv(uint32_t r0, uint32_t r1)
    {
        Emit(encode::opc(Opcode::Mv) | encode::r0(r0) | encode::r1(r1));
    }

    // illegal
    void Illegal([[maybe_unused]] uint32_t ins)
    {
        Emit(encode::opc(Opcode::Illegal));
    }

    // Directives.

    uint32_t Hi(Label label)
    {
        // %hi(label)
        if (const auto addr = AddressOf(label); addr.has_value())
        {
            // Assumes that we're only doing this for Lui which expects to shift the uimm20 that we
            // give to it.
            return *addr >> 12;
        }
        else
        {
            AddFixup<FixupType::hi20>(label);
            return 0;
        }
    }

    uint32_t Lo(Label label)
    {
        // %lo(label)
        if (const auto addr = AddressOf(label); addr.has_value())
        {
            // Assumes that we're only doing this for instructions that combine with Lui when
            // loading an absolute address.
            return *addr & 0xfff;
        }
        else
        {
            AddFixup<FixupType::lo12>(label);
            return 0;
        }
    }

    void Word(uint32_t word)
    {
        // .word(uimm32)
        Emit(word);
    }
};

static_assert(OwlHandler<Assembler>);

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

void Transcode(OwlHandler auto& a, uint32_t code)
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

    return a.Emit(encode::opc(Opcode::Illegal));
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

int main()
{
    try
    {
        // Create a 4K memory image.
        constexpr size_t memorySize = 4096;
        std::vector<uint32_t> image(memorySize / sizeof(uint32_t));

        // Load an RV32I image into a buffer.
        auto sizeBytes = std::filesystem::file_size("../a.bin");
        auto sizeWords = (sizeBytes + sizeof(uint32_t) - 1) / sizeof(uint32_t);
        auto buffer = std::vector<uint32_t>(sizeWords);
        auto ifs = std::ifstream("../a.bin");
        ifs.read(reinterpret_cast<char*>(buffer.data()), sizeBytes);
        ifs.close();

        // Transcode it to Owl-2820 and copy the result into our VM image.
        auto code = Rv32iToOwl(buffer);
        std::ranges::copy(code, image.begin());

        // Create a VM and run the image on it.
        OwlCpu cpu;
        cpu.Run(image);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
