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
    Mv
};

// System calls.
enum Syscall
{
    Exit,
    PrintFib
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

void Run(const uint32_t* code)
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
        case Opcode::Ecall: {
            const Syscall syscall = Syscall(x[a7]);
            switch (syscall)
            {
            case Syscall::Exit:
                std::cout << std::format("Exiting with status {}\n", x[a0]);
                done = true;
                break;
            case Syscall::PrintFib:
                std::cout << std::format("fib({}) = {}\n", x[a0], x[a1]);
                break;
            default:
                done = true;
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
            // Perform the comparison.
            if (x[r0(ins)] == x[r1(ins)])
            {
                nextPc = pc + offs12(ins); // Take the branch.
            }
            break;
        }

        case Opcode::Bltu: {
            // Perform the comparison.
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
// exit:
    Label exit = a.MakeLabel();
    a.BindLabel(exit);
    a.Li(a7, Syscall::Exit);    // li   a7, EXIT                ; a7 = the syscall number
    a.Ecall();                  // ecall                        ; invoke EXIT and don't return
// print_fib:
    Label print_fib = a.MakeLabel();
    a.BindLabel(print_fib);     
    a.Li(a7, Syscall::PrintFib);// li   a7, PRINT_FIB           ; a7 = the syscall number
    a.Ecall();                  // ecall                        ; invoke the PRINT_FIB syscall
    a.Ret();                    // ret                          ; return to the caller
// print_loop:
    Label print_loop = a.MakeLabel();
    a.BindLabel(print_loop);
    a.Mv(a0, s0);               // mv   a0, s0                  ; arg0 = i
    a.Mv(a1, a2);               // mv   a1, a2                  ; arg1 = current
    a.Call(print_fib);          // call print_fib               ; call print_fib
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
    a.Call(exit);               // call exit                    ; does not return
    // clang-format on

    return a.Code();
}

int main()
{
    try
    {
        auto code = Assemble();
        Run(code.data());
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
