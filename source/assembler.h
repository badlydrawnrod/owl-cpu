#pragma once

#include "endian.h"
#include "opcodes.h"

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace encode
{
    inline uint32_t opc(Opcode opcode)
    {
        return uint32_t(opcode);
    }

    inline uint32_t r0(uint32_t r)
    {
        return (r & 0x1f) << 7;
    }

    inline uint32_t r1(uint32_t r)
    {
        return (r & 0x1f) << 12;
    }

    inline uint32_t r2(uint32_t r)
    {
        return (r & 0x1f) << 17;
    }

    inline uint32_t shift(uint32_t uimm5)
    {
        return (uimm5 & 0x1f) << 17;
    }

    inline uint32_t imm12(int32_t imm12)
    {
        return static_cast<uint32_t>(imm12 << 20);
    }

    inline uint32_t offs12(int32_t offs12)
    {
        // Assumes that offs12 is pre-multiplied to a byte offset.
        return static_cast<uint32_t>(offs12 << 19) & 0xfff00000;
    }

    inline uint32_t offs20(int32_t offs20)
    {
        // Assumes that offs20 is pre-multiplied to a byte offset.
        return static_cast<uint32_t>(offs20 << 11) & 0xfffff000;
    }

    inline uint32_t uimm20(uint32_t uimm20)
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
