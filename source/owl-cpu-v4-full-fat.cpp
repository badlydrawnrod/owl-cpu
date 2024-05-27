// This is the original owl-cpu-v4.cpp, but it does too much to cover in one blog post:
//
// - it introduces syscalls
// - it introduces an implementation of memory
// - it introduces the remaining RV32I instructions
// - it expands the assembler to cater for the remaining RV32I instructions

#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <optional>
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

// Opcodes.
enum class Opcode : uint32_t
{
    Illegal = 0,
    //
    Ecall,
    Ebreak,
    //
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
    //
    Slli,
    Srli,
    Srai,
    //
    Beq,
    Bne,
    Blt,
    Bge,
    Bltu,
    Bgeu,
    //
    Jalr,
    //
    Addi,
    Slti,
    Sltiu,
    Xori,
    Ori,
    Andi,
    //
    Lb,
    Lh,
    Lw,
    Lbu,
    Lhu,
    //
    Sb,
    Sh,
    Sw,
    //
    Fence,
    //
    Jal,
    Lui,
    Auipc,
    //
    Call,
    J,
    Li,
    Mv
};

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

    uint32_t shiftimm(const uint32_t ins)
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

enum class Syscall
{
    Exit,
    PrintFib
};

void Run(const uint32_t* code, uint8_t* memory)
{
    using namespace decode;

    // Set pc and nextPc to their initial values.
    uint32_t pc = 0;     // The program counter.
    uint32_t nextPc = 0; // The address of the next instruction.

    // Set all the integer registers to zero.
    uint32_t x[32] = {}; // The integer registers.

    constexpr uint32_t wordSize = sizeof(uint32_t);

    bool done = false;

    while (!done)
    {
        // Fetch a 32-bit word from memory at the address pointed to by the program counter.
        pc = nextPc;
        nextPc += wordSize;
        const uint32_t ins = code[pc / wordSize];

        // Decode the word to extract the opcode.
        const Opcode opcode = Opcode(ins & 0x7f);

        // Dispatch it and execute it.
        switch (opcode)
        {
            // System instructions.

        case Opcode::Ecall: {
            const auto syscall = Syscall(x[a0]);
            switch (syscall)
            {
            case Syscall::Exit:
                std::cout << std::format("Exiting with status {}\n", x[a1]);
                done = true;
                break;
            case Syscall::PrintFib:
                std::cout << std::format("fib({}) = {}\n", x[a1], x[a2]);
                break;
            default:
                done = true;
            }
            break;
        }

        case Opcode::Ebreak: {
            done = true;
            break;
        }

            // Integer register-register instructions.

        case Opcode::Add: {
            // r0 <- r1 + r2
            x[r0(ins)] = x[r1(ins)] + x[r2(ins)];
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Sub: {
            // r0 <- r1 - r2
            x[r0(ins)] = x[r1(ins)] - x[r2(ins)];
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Sll: {
            // r0 <- r1 << (r2 % 32)
            x[r0(ins)] = x[r1(ins)] << (x[r2(ins)] % 32);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Slt: {
            // r0 <- (r1 < r2) ? 1 : 0 (signed integer comparison)
            const int32_t sr1 = static_cast<int32_t>(x[r1(ins)]);
            const int32_t sr2 = static_cast<int32_t>(x[r2(ins)]);
            x[r0(ins)] = sr1 < sr2 ? 1 : 0;
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Sltu: {
            // r0 <- (r1 < r2) ? 1 : 0 (unsigned integer comparison)
            x[r0(ins)] = x[r1(ins)] < x[r2(ins)] ? 1 : 0;
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Xor: {
            // r0 <- r1 ^ r2
            x[r0(ins)] = x[r1(ins)] ^ x[r2(ins)];
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Srl: {
            // r0 <- r1 >> (r2 % 32) (shift right logical)
            x[r0(ins)] = x[r1(ins)] >> (x[r2(ins)] % 32);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Sra: {
            // r0 <- r1 >> (r2 % 32) (shift right arithmetic)
            const int32_t sr1 = static_cast<int32_t>(x[r1(ins)]);
            const int32_t shift = static_cast<int32_t>(x[r2(ins)]) % 32;
            x[r0(ins)] = static_cast<uint32_t>(sr1 >> shift);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Or: {
            // r0 <- r1 | r2
            x[r0(ins)] = x[r1(ins)] | x[r2(ins)];
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::And: {
            // r0 <- r1 & r2
            x[r0(ins)] = x[r1(ins)] & x[r2(ins)];
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

            // Immediate shift instructions.

        case Opcode::Slli: {
            // r0 <- r1 << shiftimm
            x[r0(ins)] = x[r1(ins)] << shiftimm(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Srli: {
            // r0 <- r1 >> shiftimm (shift right logical)
            const int32_t sr1 = static_cast<int32_t>(x[r1(ins)]);
            const int32_t shift = shiftimm(ins);
            x[r0(ins)] = static_cast<uint32_t>(sr1 >> shift);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Srai: {
            // r0 <- r1 >> shiftimm (shift right logical)
            x[r0(ins)] = x[r1(ins)] >> shiftimm(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

            // Conditional branch instructions.

        case Opcode::Beq: {
            // pc <- pc + ((r0 == r1) ? offs12 : 4)
            if (x[r0(ins)] == x[r1(ins)])
            {
                nextPc = pc + offs12(ins); // Take the branch.
            }
            break;
        }

        case Opcode::Bne: {
            // pc <- pc + ((r0 != r1) ? offs12 : 4)
            if (x[r0(ins)] != x[r1(ins)])
            {
                nextPc = pc + offs12(ins); // Take the branch.
            }
            break;
        }

        case Opcode::Blt: {
            // pc <- pc + ((r0 < r1) ? offs12 : 4) (signed integer comparison)
            int32_t sr0 = static_cast<int32_t>(x[r0(ins)]);
            int32_t sr1 = static_cast<int32_t>(x[r1(ins)]);
            if (sr0 < sr1)
            {
                nextPc = pc + offs12(ins); // Take the branch.
            }
            break;
        }

        case Opcode::Bge: {
            // pc <- pc + ((r0 >= r1) ? offs12 : 4) (signed integer comparison)
            int32_t sr0 = static_cast<int32_t>(x[r0(ins)]);
            int32_t sr1 = static_cast<int32_t>(x[r1(ins)]);
            if (sr0 >= sr1)
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

        case Opcode::Bgeu: {
            // pc <- pc + ((r0 >= r1) ? offs12 : 4) (unsigned integer comparison)
            if (x[r0(ins)] >= x[r1(ins)])
            {
                nextPc = pc + offs12(ins); // Take the branch.
            }
            break;
        }

            // Misc

        case Opcode::Jalr: {
            // r0 <- pc + 4, pc <- r1 + offs12
            const int32_t r1Before = x[r1(ins)]; // r0 and r1 may be the same register.
            x[r0(ins)] = nextPc;
            nextPc = r1Before + offs12(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

            // Integer register-immediate instructions.

        case Opcode::Addi: {
            // r0 <- r1 + imm12
            x[r0(ins)] = x[r1(ins)] + imm12(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Slti: {
            // r0 <- (r1 < imm12) ? 1 : 0 (signed integer comparison)
            const int32_t sr1 = static_cast<int32_t>(x[r1(ins)]);
            const int32_t simm12 = static_cast<int32_t>(x[imm12(ins)]);
            x[r0(ins)] = sr1 < simm12 ? 1 : 0;
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Sltiu: {
            // r0 <- (r1 < imm12) ? 1 : 0 (unsigned integer comparison)
            x[r0(ins)] = x[r1(ins)] < imm12(ins) ? 1 : 0;
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Xori: {
            // r0 <- r1 ^ imm12
            x[r0(ins)] = x[r1(ins)] ^ imm12(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Ori: {
            // r0 <- r1 | imm12
            x[r0(ins)] = x[r1(ins)] | imm12(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Andi: {
            // r0 <- r1 & imm12
            x[r0(ins)] = x[r1(ins)] & imm12(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

            // Load instructions.

        case Opcode::Lb: {
            // r0 <- sext(memory(r1 + imm12))
            const uint32_t addr = r1(ins) + imm12(ins);
            const int32_t signExtendedByte = static_cast<int8_t>(memory[addr]);
            x[r0(ins)] = static_cast<uint32_t>(signExtendedByte);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lh: {
            // r0 <- sext(memory16(r1 + imm12))
            const uint32_t addr = r1(ins) + imm12(ins);
            const int32_t signExtendedHalfWord =
                    static_cast<int16_t>(memory[addr] + (memory[addr + 1] << 8));
            x[r0(ins)] = static_cast<uint32_t>(signExtendedHalfWord);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lw: {
            // r0 <- memory32(r1 + imm12)
            const uint32_t addr = r1(ins) + imm12(ins);
            const uint32_t word = memory[addr] | (memory[addr + 1] << 8) | (memory[addr + 2] << 16)
                    | (memory[addr + 3] << 24);
            x[r0(ins)] = word;
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lbu: {
            // r0 <- zext(memory(r1 + imm12))
            const uint32_t addr = r1(ins) + imm12(ins);
            x[r0(ins)] = memory[addr];
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lhu: {
            // r0 <- zext(memory16(r1 + imm12))
            const uint32_t addr = r1(ins) + imm12(ins);
            const uint32_t zeroExtendedHalfWord = memory[addr] | (memory[addr + 1] << 8);
            x[r0(ins)] = zeroExtendedHalfWord;
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

            // Store instructions.

        case Opcode::Sb: {
            // memory(r0 + imm12) <- r1[7:0]
            const uint32_t addr = r0(ins) + imm12(ins);
            memory[addr] = r1(ins) & 0xff;
            break;
        }

        case Opcode::Sh: {
            // memory16(r0 + imm12) <- r1[15:0]
            const uint32_t addr = r0(ins) + imm12(ins);
            const uint32_t v = r1(ins);
            memory[addr] = v & 0xff;
            memory[addr + 1] = (v >> 8) & 0xff;
            break;
        }

        case Opcode::Sw: {
            // memory32(r0 + imm12) <- r1
            const uint32_t addr = r0(ins) + imm12(ins);
            const uint32_t v = r1(ins);
            memory[addr] = v & 0xff;
            memory[addr + 1] = (v >> 8) & 0xff;
            memory[addr + 2] = (v >> 16) & 0xff;
            memory[addr + 3] = (v >> 24) & 0xff;
            break;
        }

            // Cache/memory instructions.

        case Opcode::Fence: {
            // Do nothing.
            break;
        }

            // Misc.

        case Opcode::Jal: {
            // r0 <- pc + 4, pc <- pc + offs20
            x[r0(ins)] = nextPc;
            nextPc = pc + offs20(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Lui: {
            // r0 <- uimm20
            x[r0(ins)] = uimm20(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Auipc: {
            // r0 <- pc + imm_u
            x[r0(ins)] = pc + uimm20(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

            // Other instructions - not RV32i.

        case Opcode::Call: {
            // ra <- pc + 4, pc <- pc + offs20
            x[1] = nextPc;
            nextPc = pc + offs20(ins);
            break;
        }

        case Opcode::J: {
            // pc <- pc + offs20
            nextPc = pc + offs20(ins);
            break;
        }

        case Opcode::Li: {
            // r0 <- imm12
            x[r0(ins)] = imm12(ins);
            x[0] = 0; // Ensure x0 is always zero.
            break;
        }

        case Opcode::Mv: {
            // r0 <- r1
            x[r0(ins)] = x[r1(ins)];
            x[0] = 0; // Ensure x0 is always zero.
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

    uint32_t shiftimm(uint32_t uimm5)
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
        offs20
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
                const auto offset = address - fixup.target;
                if (fixup.type == FixupType::offs12)
                {
                    ResolveFixup<FixupType::offs12>(fixup.target, offset);
                }
                else if (fixup.type == FixupType::offs20)
                {
                    ResolveFixup<FixupType::offs20>(fixup.target, offset);
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
        code_.push_back(u);
        current_ += 4; // 4 bytes per instruction.
    }

    // Syscall instructions.

    void Ecall()
    {
        Emit(encode::opc(Opcode::Ecall));
    }

    void Ebreak()
    {
        Emit(encode::opc(Opcode::Ebreak));
    }

    // Integer register-register instructions.

    template<Opcode opcode>
    void EmitRegReg(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        Emit(encode::opc(opcode) | encode::r0(r0) | encode::r1(r1) | encode::r2(r2));
    }

    void Add(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        EmitRegReg<Opcode::Add>(r0, r1, r2);
    }

    void Sub(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        EmitRegReg<Opcode::Sub>(r0, r1, r2);
    }

    void Sll(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        EmitRegReg<Opcode::Sll>(r0, r1, r2);
    }

    void Slt(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        EmitRegReg<Opcode::Slt>(r0, r1, r2);
    }

    void Sltu(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        EmitRegReg<Opcode::Sltu>(r0, r1, r2);
    }

    void Xor(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        EmitRegReg<Opcode::Xor>(r0, r1, r2);
    }

    void Srl(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        EmitRegReg<Opcode::Srl>(r0, r1, r2);
    }

    void Sra(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        EmitRegReg<Opcode::Sra>(r0, r1, r2);
    }

    void Or(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        EmitRegReg<Opcode::Or>(r0, r1, r2);
    }

    void And(uint32_t r0, uint32_t r1, uint32_t r2)
    {
        EmitRegReg<Opcode::And>(r0, r1, r2);
    }

    // Immediate shift insturctions.

    template<Opcode opcode>
    void EmitShiftImm(uint32_t r0, uint32_t r1, uint32_t uimm5)
    {
        Emit(encode::opc(opcode) | encode::r0(r0) | encode::r1(r1) | encode::shiftimm(uimm5));
    }

    void Slli(uint32_t r0, uint32_t r1, uint32_t uimm5)
    {
        EmitShiftImm<Opcode::Slli>(r0, r1, uimm5);
    }

    void Srli(uint32_t r0, uint32_t r1, uint32_t uimm5)
    {
        EmitShiftImm<Opcode::Srli>(r0, r1, uimm5);
    }

    void Srai(uint32_t r0, uint32_t r1, uint32_t uimm5)
    {
        EmitShiftImm<Opcode::Srai>(r0, r1, uimm5);
    }

    // Branches with offs12.

    template<Opcode opcode>
    void EmitBranch(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        Emit(encode::opc(opcode) | encode::r0(r0) | encode::r1(r1) | encode::offs12(offs12));
    }

    template<Opcode opcode>
    void EmitBranch(uint32_t r0, uint32_t r1, Label label)
    {
        if (const auto addr = AddressOf(label); addr.has_value())
        {
            EmitBranch<opcode>(r0, r1, *addr - Current());
        }
        else
        {
            AddFixup<FixupType::offs12>(label);
            EmitBranch<opcode>(r0, r1, 0);
        }
    }

    void Beq(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        EmitBranch<Opcode::Beq>(r0, r1, offs12);
    }

    void Beq(uint32_t r0, uint32_t r1, Label label)
    {
        EmitBranch<Opcode::Beq>(r0, r1, label);
    }

    void Bltu(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        EmitBranch<Opcode::Bltu>(r0, r1, offs12);
    }

    void Bltu(uint32_t r0, uint32_t r1, Label label)
    {
        EmitBranch<Opcode::Bltu>(r0, r1, label);
    }

    void Jalr(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        EmitBranch<Opcode::Jalr>(r0, r1, offs12);
    }

    void Jalr(uint32_t r0, uint32_t r1, Label label)
    {
        EmitBranch<Opcode::Jalr>(r0, r1, label);
    }

    // Integer register-immediate instructions.

    template<Opcode opcode>
    void EmitRegImm(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        Emit(encode::opc(opcode) | encode::r0(r0) | encode::r1(r1) | encode::imm12(imm12));
    }

    void Addi(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        EmitRegImm<Opcode::Addi>(r0, r1, imm12);
    }

    void Slti(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        EmitRegImm<Opcode::Slti>(r0, r1, imm12);
    }

    void Xori(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        EmitRegImm<Opcode::Xori>(r0, r1, imm12);
    }

    void Ori(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        EmitRegImm<Opcode::Ori>(r0, r1, imm12);
    }

    void Andi(uint32_t r0, uint32_t r1, int32_t imm12)
    {
        EmitRegImm<Opcode::Andi>(r0, r1, imm12);
    }

    // Load instructions.

    template<Opcode opcode>
    void EmitLoad(uint32_t r0, int32_t imm12)
    {
        Emit(encode::opc(opcode) | encode::r0(r0) | encode::imm12(imm12));
    }

    void Lb(uint32_t r0, int32_t imm12)
    {
        EmitLoad<Opcode::Lb>(r0, imm12);
    }

    void Lh(uint32_t r0, int32_t imm12)
    {
        EmitLoad<Opcode::Lh>(r0, imm12);
    }

    void Lw(uint32_t r0, int32_t imm12)
    {
        EmitLoad<Opcode::Lw>(r0, imm12);
    }

    void Lbu(uint32_t r0, int32_t imm12)
    {
        EmitLoad<Opcode::Lbu>(r0, imm12);
    }

    void Lhu(uint32_t r0, int32_t imm12)
    {
        EmitLoad<Opcode::Lhu>(r0, imm12);
    }

    // Store instructions.

    template<Opcode opcode>
    void EmitStore(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        Emit(encode::opc(opcode) | encode::r0(r0) | encode::imm12(imm12) | encode::r1(r1));
    }

    void Sb(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        EmitStore<Opcode::Sb>(r0, imm12, r1);
    }

    void Sh(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        EmitStore<Opcode::Sh>(r0, imm12, r1);
    }

    void Sw(uint32_t r0, int32_t imm12, uint32_t r1)
    {
        EmitStore<Opcode::Sw>(r0, imm12, r1);
    }

    // Cache/memory instructions.

    void Fence()
    {
        Emit(encode::opc(Opcode::Fence));
    }

    // Misc.

    void Jal(uint32_t r0, int32_t offs20)
    {
        Emit(encode::opc(Opcode::Jal) | encode::r0(r0) | encode::offs20(offs20));
    }

    void Jal(uint32_t r0, Label label)
    {
        if (const auto addr = AddressOf(label); addr.has_value())
        {
            Jal(r0, *addr - Current());
        }
        else
        {
            AddFixup<FixupType::offs20>(label);
            Jal(r0, 0);
        }
    }

    void Lui(uint32_t r0, uint32_t uimm20)
    {
        Emit(encode::opc(Opcode::Lui) | encode::r0(r0) | encode::uimm20(uimm20));
    }

    void Auipc(uint32_t r0, uint32_t uimm20)
    {
        Emit(encode::opc(Opcode::Auipc) | encode::r0(r0) | encode::uimm20(uimm20));
    }

    // Other instructions - would be RV32I pseudo-instructions.

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

    void Mv(uint32_t r0, uint32_t r1)
    {
        // mv r0, r1
        Emit(encode::opc(Opcode::Mv) | encode::r0(r0) | encode::r1(r1));
    }
};

std::vector<uint32_t> Assemble()
{
    Assembler a;

    // clang-format off
// main:
    a.Li(s0, 0);                // li   s0, 0                   ; i = 0
    a.Li(s2, 2);                // li   s2, 2                   ; s2 = 2
    a.Li(s3, 48);               // li   s3, 48                  ; s3 = 48
    a.Li(s4, 1);                // li   s4, 1                   ; s4 = 1
    Label fib = a.MakeLabel();
    a.J(fib);                   // j    fib                     ; go to fib
// print_loop:
    Label print_loop = a.MakeLabel();
    a.BindLabel(print_loop);
    a.Li(a0, int32_t(Syscall::PrintFib)); // li   a0, PRINT_FIB           ; arg0 = the syscall number
    a.Mv(a1, s0);               // mv   a1, s0                  ; arg1 = i (arg2 contains current)
    a.Ecall();                  // ecall                        ; invoke the PRINT_FIB syscall
    a.Addi(s0, s0, 1);          // addi s0, s0, 1               ; i = i + 1
    Label done = a.MakeLabel();
    a.Beq(s0, s3, done);        // beq  s0, s3, done            ; if i == 48 go to done
// fib:
    a.BindLabel(fib);
    a.Mv(a2, s0);               // mv   a2, s0                  ; current = i
    a.Bltu(s0, s2, print_loop); // bltu s0, s2, print_loop      ; if i < 2 go to print_loop
    a.Li(a0, 0);                // li   a0, 0                   ; previous = 0
    a.Li(a2, 1);                // li   a2, 1                   ; current = 1
    a.Mv(a1, s0);               // mv   a1, s0                  ; n = i
// fib_loop:
    Label fib_loop = a.MakeLabel();
    a.BindLabel(fib_loop);
    a.Mv(a3, a2);               // mv   a3, a2                  ; tmp = current
    a.Addi(a1, a1, -1);         // addi a1, a1, -1              ; n = n - 1
    a.Add(a2, a0, a2);          // add  a2, a0, a2              ; current = current + prev
    a.Mv(a0, a3);               // mv   a0, a3                  ; previous = tmp
    a.Bltu(s4, a1, fib_loop);   // bltu s4, a1, fib_loop        ; if n > 1 go to fib_loop
    a.J(print_loop);            // j    print_loop              ; go to print_loop
// done:
    a.BindLabel(done);
    a.Li(a0, 0);                // li   a0, 0                   ; set the return value of main() to 0

    // Exit.
    a.Li(a0, int32_t(Syscall::Exit));// li   a0, EXIT                ; arg0 = the syscall number
    a.Li(a1, 0);                // li   a1, 0                   ; exit status 0
    a.Ecall();                  // ecall                        ; invoke the PRINT_FIB syscall

    // clang-format on

    return a.Code();
}

int main()
{
    try
    {
        constexpr size_t memSize = 1024;
        std::vector<uint8_t> memory(memSize);
        auto code = Assemble();
        Run(code.data(), memory.data());
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
