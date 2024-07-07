#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <format>
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
    Ecall,
    Add,
    Addi,
    Beq,
    Bltu,
    Call,
    Ret,
    J,
    Li,
    Lui,
    Mv,
    Lb,
    Lbu,
    Lh,
    Lhu,
    Lw,
    Sb,
    Sh,
    Sw
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

void Run(std::span<uint32_t> image)
{
    using namespace decode;

    // Get a read-only, word addressable view of the image for fetching instructions.
    const auto code = image;

    // Get a byte-addressable view of the image for memory accesses.
    auto memory = std::as_writable_bytes(image);

    // Set pc and nextPc to their initial values.
    uint32_t pc = 0;     // The program counter.
    uint32_t nextPc = 0; // The address of the next instruction.

    // Set all the integer registers to zero.
    uint32_t x[32] = {}; // The integer registers.

    // Set the stack pointer to the end of memory.
    x[sp] = uint32_t(memory.size());

    constexpr uint32_t wordSize = sizeof(uint32_t);

    bool done = false;

    while (!done)
    {
        // Fetch a 32-bit word from the address pointed to by the program counter.
        pc = nextPc;
        nextPc += wordSize;
        const uint32_t ins = AsLE(code[pc / wordSize]);

        // Decode the word to extract the opcode.
        const Opcode opcode = Opcode(ins & 0x7f);

        // Dispatch it and execute it.
        switch (opcode)
        {
        case Opcode::Ecall: {
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
            break;
        }

        case Opcode::Add: {
            x[r0(ins)] = x[r1(ins)] + x[r2(ins)]; // Add the two registers.
            x[0] = 0;                             // Ensure x0 is always zero.
            break;
        }

        case Opcode::Addi: {
            x[r0(ins)] = x[r1(ins)] + imm12(ins); // Perform the addition.
            x[0] = 0;                             // Ensure x0 is always zero.
            break;
        }

        case Opcode::Beq: {
            // pc <- pc + ((r0 == r1) ? offs12 : 4)
            if (x[r0(ins)] == x[r1(ins)])
            {
                nextPc = pc + offs12(ins); // Take the branch.
            }
            break;
        }

        case Opcode::Bltu: {
            // pc <- pc + ((r0 < r1) ? offs12 : 4) (unsigned integer comparison)
            if (x[r0(ins)] < x[r1(ins)])
            {
                nextPc = pc + offs12(ins); // Take the branch.
            }
            break;
        }

        case Opcode::Call: {
            // ra <- pc + 4, pc <- pc + offs20
            x[ra] = nextPc;            // Save the return address in ra.
            nextPc = pc + offs20(ins); // Make the call.
            break;
        }

        case Opcode::Ret: {
            // pc <- ra
            nextPc = x[ra]; // Return to ra.
            break;
        }

        case Opcode::J: {
            nextPc = pc + offs20(ins); // Branch.
            break;
        }

        case Opcode::Li: {
            x[r0(ins)] = imm12(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lui: {
            x[r0(ins)] = uimm20(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Mv: {
            x[r0(ins)] = x[r1(ins)];
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lb: {
            // r0 <- sext(memory8(r1 + imm12))
            const uint32_t addr = x[r1(ins)] + imm12(ins);
            const int32_t signExtendedByte = static_cast<int8_t>(Read8(memory, addr));
            x[r0(ins)] = signExtendedByte;
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lbu: {
            // r0 <- zext(memory(r1 + imm12))
            const uint32_t addr = x[r1(ins)] + imm12(ins);
            const uint32_t zeroExtendedByte = static_cast<uint8_t>(Read8(memory, addr));
            x[r0(ins)] = zeroExtendedByte;
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lh: {
            // r0 <- sext(memory16(r1 + imm12))
            const uint32_t addr = x[r1(ins)] + imm12(ins);
            const int32_t signExtendedHalfWord = static_cast<int16_t>(Read16(memory, addr));
            x[r0(ins)] = signExtendedHalfWord;
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lhu: {
            // r0 <- zext(memory16(r1 + imm12))
            const uint32_t addr = x[r1(ins)] + imm12(ins);
            const uint32_t zeroExtendedHalfWord = static_cast<uint16_t>(Read16(memory, addr));
            x[r0(ins)] = zeroExtendedHalfWord;
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lw: {
            // r0 <- memory32(r1 + imm12)
            const uint32_t addr = x[r1(ins)] + imm12(ins);
            x[r0(ins)] = Read32(memory, addr);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Sb: {
            // memory8(r1 + imm12) <- r0[7:0]
            const uint32_t addr = x[r1(ins)] + imm12(ins);
            const std::byte byte = static_cast<std::byte>(x[r0(ins)]);
            Write8(memory, addr, byte);
            break;
        }

        case Opcode::Sh: {
            // memory16(r1 + imm12) <- r0[15:0]
            const uint32_t addr = x[r1(ins)] + imm12(ins);
            const uint16_t halfWord = static_cast<uint16_t>(x[r0(ins)]);
            Write16(memory, addr, halfWord);
            break;
        }

        case Opcode::Sw: {
            // memory32(r1 + imm12) <- r0
            const uint32_t addr = x[r1(ins)] + imm12(ins);
            const uint32_t word = x[r0(ins)];
            Write32(memory, addr, word);
            break;
        }

        // Stop the loop if we hit an illegal instruction.
        default:
            // If we don't recognise the opcode then by default we have an illegal instruction.
            done = true;
        }
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

    void Ecall()
    {
        Emit(encode::opc(Opcode::Ecall));
    }

    void Add(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        // add r0, r1, r2
        Emit(encode::opc(Opcode::Add) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    void Addi(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        // addi r0, r1, imm12
        Emit(encode::opc(Opcode::Addi) | encode::r0(r0) | encode::r1(r1) | encode::imm12(imm12));
    }

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

    void Beq(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        Branch<Opcode::Beq>(r0, r1, offs12);
    }

    void Beq(uint32_t r0, uint32_t r1, Label label)
    {
        Branch<Opcode::Beq>(r0, r1, label);
    }

    void Bltu(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        Branch<Opcode::Bltu>(r0, r1, offs12);
    }

    void Bltu(uint32_t r0, uint32_t r1, Label label)
    {
        Branch<Opcode::Bltu>(r0, r1, label);
    }

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

    void Call(int32_t offs20)
    {
        Jump<Opcode::Call>(offs20);
    }

    void Call(Label label)
    {
        Jump<Opcode::Call>(label);
    }

    void Ret()
    {
        Emit(encode::opc(Opcode::Ret));
    }

    void J(int32_t offs20)
    {
        Jump<Opcode::J>(offs20);
    }

    void J(Label label)
    {
        Jump<Opcode::J>(label);
    }

    void Li(uint32_t r0, int32_t imm12)
    {
        // li r0, imm12
        Emit(encode::opc(Opcode::Li) | encode::r0(r0) | encode::imm12(imm12));
    }

    void Lui(uint32_t r0, uint32_t uimm20)
    {
        // lui r0, uimm20
        Emit(encode::opc(Opcode::Lui) | encode::r0(r0) | encode::uimm20(uimm20));
    }

    void Mv(uint32_t r0, uint32_t r1)
    {
        // mv r0, r1
        Emit(encode::opc(Opcode::Mv) | encode::r0(r0) | encode::r1(r1));
    }

    void Lb(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // lb r0, imm12(r1)
        Emit(encode::opc(Opcode::Lb) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    void Lbu(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // lbu r0, imm12(r1)
        Emit(encode::opc(Opcode::Lbu) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    void Lh(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // lh r0, imm12(r1)
        Emit(encode::opc(Opcode::Lh) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    void Lhu(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // lhu r0, imm12(r1)
        Emit(encode::opc(Opcode::Lhu) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    void Lw(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // lw r0, imm12(r1)
        Emit(encode::opc(Opcode::Lw) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    void Sb(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // sb r0, imm12(r1)
        Emit(encode::opc(Opcode::Sb) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    void Sh(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // sh r0, imm12(r1)
        Emit(encode::opc(Opcode::Sh) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    void Sw(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        // sw r0, imm12(r1)
        Emit(encode::opc(Opcode::Sw) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

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

std::vector<uint32_t> Assemble()
{
    Assembler a;

    // clang-format off
// start:
    Label main = a.MakeLabel();
    a.Call(main);

    // Invoke the `Exit` syscall. There's no coming back from this.
    a.Li(a7, Syscall::Exit);    // li      a7, 0
    a.Ecall();                  // ecall

// main:
    a.BindLabel(main);
    a.Addi(sp, sp, -32);        // addi    sp, sp, -32             ; reserve space on the stack

    // save registers s0 - s3 and the return address register ra onto the stack
    a.Sw(s0, 24, sp);           // sw      s0, 24(sp)
    a.Sw(s1, 20, sp);           // sw      s1, 20(sp)
    a.Sw(s2, 16, sp);           // sw      s2, 16(sp)
    a.Sw(s3, 12, sp);           // sw      s3, 12(sp)
    a.Sw(ra, 28, sp);           // sw      ra, 28(sp)

    // ; s1 = the address of the start of the lookup table
    Label lut = a.MakeLabel();
    a.Lui(s1, a.Hi(lut));       // lui     s1, %hi(lut)
    a.Addi(s1, s1, a.Lo(lut));  // addi    s1, s1, %lo(lut)

    a.Li(s0, 0);                // li      s0, 0                   ; i = 0
    a.Li(s2, 48);               // li      s2, 48                  ; s2 = 48
// print_loop:
    Label print_loop = a.MakeLabel();
    a.BindLabel(print_loop);
    a.Lw(a1, 0, s1);            // lw      a1, 0(s1)               ; arg1 = *s1
    a.Mv(a0, s0);               // mv      a0, s0                  ; arg0 = i
    a.Addi(s0, s0, 1);          // addi    s0, s0, 1               ; i++
    Label print_fib = a.MakeLabel();
    a.Call(print_fib);          // call    print_fib
    a.Addi(s1, s1, 4);          // addi    s1, s1, 4               ; bump s1 to the next address in the lookup table
    a.Bltu(s0, s2, print_loop); // bltu    s0, s2, print_loop      ; if i < 48 go to print_loop
        
    // restore the return address register ra and registers s0 - s3 from the stack
    a.Lw(ra, 28, sp);           // lw      ra, 28(sp)
    a.Lw(s0, 24, sp);           // lw      s0, 24(sp)
    a.Lw(s1, 20, sp);           // lw      s1, 20(sp)
    a.Lw(s2, 16, sp);           // lw      s2, 16(sp)
    a.Lw(s3, 12, sp);           // lw      s3, 12(sp)

    a.Li(a0, 0);                // li      a0, 0                   ; set the return value to zero

    a.Addi(sp, sp, 32);         // addi    sp, sp, 32              ; free the reserved space back to the stack
    a.Ret();                    // ret                             ; return from main

// print_fib:
    a.BindLabel(print_fib);
    
    // Invoke the `PrintFib` syscall.
    a.Li(a7, Syscall::PrintFib);// li       a7, 1
    a.Ecall();                  // ecall

    a.Ret();                    // ret                             ; return from print_fib

// lut:
    a.BindLabel(lut);
    a.Word(0);
    a.Word(1);
    a.Word(1);
    a.Word(2);
    a.Word(3);
    a.Word(5);
    a.Word(8);
    a.Word(13);
    a.Word(21);
    a.Word(34);
    a.Word(55);
    a.Word(89);
    a.Word(144);
    a.Word(233);
    a.Word(377);
    a.Word(610);
    a.Word(987);
    a.Word(1597);
    a.Word(2584);
    a.Word(4181);
    a.Word(6765);
    a.Word(10946);
    a.Word(17711);
    a.Word(28657);
    a.Word(46368);
    a.Word(75025);
    a.Word(121393);
    a.Word(196418);
    a.Word(317811);
    a.Word(514229);
    a.Word(832040);
    a.Word(1346269);
    a.Word(2178309);
    a.Word(3524578);
    a.Word(5702887);
    a.Word(9227465);
    a.Word(14930352);
    a.Word(24157817);
    a.Word(39088169);
    a.Word(63245986);
    a.Word(102334155);
    a.Word(165580141);
    a.Word(267914296);
    a.Word(433494437);
    a.Word(701408733);
    a.Word(1134903170);
    a.Word(1836311903);
    a.Word(2971215073);
    // clang-format on

    return a.Code();
}

int main()
{
    try
    {
        // Create a 4K memory image.
        constexpr size_t memorySize = 4096;
        std::vector<uint32_t> image(memorySize / sizeof(uint32_t));

        // Assemble our program and copy it to the start of the memory image.
        auto code = Assemble();
        std::ranges::copy(code, image.begin());

        Run(image);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
