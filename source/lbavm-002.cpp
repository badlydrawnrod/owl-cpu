#include <cstdint>
#include <format>
#include <iostream>
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
    Add,
    Addi,
    Beq,
    Bltu,
    Call,
    J,
    Li,
    Lui,
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
            // Do a hard-coded printf().
            std::cout << std::format("fib({}) = {}\n", x[a1], x[a2]);
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

class Assembler
{
    std::vector<uint32_t> code_;

public:
    const std::vector<uint32_t>& Code() const
    {
        return code_;
    }

    void Emit(uint32_t u)
    {
        code_.push_back(u);
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

    void Beq(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        // beq r0, r1, offs12
        Emit(encode::opc(Opcode::Beq) | encode::r0(r0) | encode::r1(r1) | encode::offs12(offs12));
    }

    void Bltu(uint32_t r0, uint32_t r1, int32_t offs12)
    {
        // bltu r0, r1, offs12
        Emit(encode::opc(Opcode::Bltu) | encode::r0(r0) | encode::r1(r1) | encode::offs12(offs12));
    }

    void Call(int32_t offs20)
    {
        // call offs20
        Emit(encode::opc(Opcode::Call) | encode::offs20(offs20));
    }

    void J(int32_t offs20)
    {
        // j offs20
        Emit(encode::opc(Opcode::J) | encode::offs20(offs20));
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

// clang-format off
std::vector<uint32_t> Assemble()
{
    Assembler a;

    // Offsets to labels.
    const int32_t fib = 24;
    const int32_t print_loop1 = -24;
    const int32_t print_loop2 = -60;
    const int32_t printf = 0; // No value, because we're going to cheat.
    const int32_t done = 48;
    const int32_t fib_loop = -16;
// main:
    a.Li(s0, 0);                // li   s0, 0                   ; i = 0
    a.Li(s2, 2);                // li   s2, 2                   ; s2 = 2
    a.Lui(a0, 1);               // lui  a0, %hi(format_str)
    a.Addi(s1, a0, -548);       // addi s1, a0, %lo(format_str) ; s1 = the address of the printf format string
    a.Li(s3, 48);               // li   s3, 48                  ; s3 = 48
    a.Li(s4, 1);                // li   s4, 1                   ; s4 = 1
    a.J(fib);                   // j    fib                     ; go to fib
// print_loop:
    a.Mv(a0, s1);               // mv   a0, s1                  ; arg0 = the address of the printf format string
    a.Mv(a1, s0);               // mv   a1, s0                  ; arg1 = i (arg2 contains current)
    a.Call(printf);             // call printf                  ; call printf
    a.Addi(s0, s0, 1);          // addi s0, s0, 1               ; i = i + 1
    a.Beq(s0, s3, done);        // beq  s0, s3, done            ; if i == 48 go to done
// fib:
    a.Mv(a2, s0);               // mv   a2, s0                  ; current = i
    a.Bltu(s0, s2, print_loop1);// bltu s0, s2, print_loop      ; if i < 2 go to print_loop
    a.Li(a0, 0);                // li   a0, 0                   ; previous = 0
    a.Li(a2, 1);                // li   a2, 1                   ; current = 1
    a.Mv(a1, s0);               // mv   a1, s0                  ; n = i
// fib_loop:
    a.Mv(a3, a2);               // mv   a3, a2                  ; tmp = current
    a.Addi(a1, a1, -1);         // addi a1, a1, -1              ; n = n - 1
    a.Add(a2, a0, a2);          // add  a2, a0, a2              ; current = current + prev
    a.Mv(a0, a3);               // mv   a0, a3                  ; previous = tmp
    a.Bltu(s4, a1, fib_loop);   // bltu s4, a1, fib_loop        ; if n > 1 go to fib_loop
    a.J(print_loop2);           // j    print_loop              ; go to print_loop
// done:
    a.Li(a0, 0);                // li   a0, 0                   ; set the return value of main() to 0

    // Emit an illegal instruction so that we have something to stop us.
    a.Emit(0);

    return a.Code();
}
// clang-format on

int main()
{
    auto code = Assemble();
    Run(code.data());
}
