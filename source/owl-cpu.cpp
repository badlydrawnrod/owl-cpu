#include <cstdint>
#include <format>
#include <iostream>
#include <string>
#include <vector>

struct Owl
{
    uint32_t pc;
    uint32_t x[32];
};

// clang-format off
enum
{
    zero, ra, sp, gp, tp,  t0,  t1, t2, s0, s1, a0,
    a1,   a2, a3, a4, a5,  a6,  a7, s2, s3, s4, s5,
    s6,   s7, s8, s9, s10, s11, t3, t4, t5, t6
};
// clang-format on

static const char* regnames[] = {"zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
                                 "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
                                 "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

enum class Opcode
{
    Illegal,
    Add,
    Addi,
    Beq,
    Bltu,
    Call,
    J,
    Li,
    Lui,
    Mv,
};

struct Reg
{
    // +-----+--------+-----+----+-----+
    // | rs2 | unused | rs1 | rd | opc |
    // |   5 |     10 |   5 |  5 |   7 |
    // +-----+--------+-----+----+-----+
    Reg(Opcode opc, uint32_t rd, uint32_t rs1, uint32_t rs2) : v{(rs2 << 27) | (rs1 << 12) | (rd << 7) | uint32_t(opc)}
    {
    }

    operator uint32_t() const { return v; }

    auto opc() const -> Opcode { return Opcode(v & 0x7f); }
    auto rd() const -> uint32_t { return (v >> 7) & 0x1f; }
    auto rs1() const -> uint32_t { return (v >> 12) & 0x1f; }
    auto rs2() const -> uint32_t { return (v >> 27) & 0x1f; }

private:
    uint32_t v;
};

struct Immediate
{
    // +-------+--------+----+----+-----+
    // | imm12 | unused | rs | rd | opc |
    // |    12 |      3 |  5 |  5 |   7 |
    // +-------+--------+----+----+-----+
    Immediate(Opcode opc, uint32_t rd, uint32_t rs, uint32_t imm12)
        : v{(imm12 << 20) | (rs << 12) | (rd << 7) | uint32_t(opc)}
    {
    }

    operator uint32_t() const { return v; }

    auto opc() const -> Opcode { return Opcode(v & 0x7f); }
    auto rd() const -> uint32_t { return (v >> 7) & 0x1f; }
    auto rs() const -> uint32_t { return (v >> 12) & 0x1f; }
    auto sximm12() const -> uint32_t { return uint32_t(int32_t(v) >> 20); }

private:
    uint32_t v{};
};

struct Branch
{
    // +--------+--------+-----+-----+-----+
    // | offs12 | unused | rs2 | rs1 | opc |
    // |     12 |      3 |   5 |   5 |   7 |
    // +--------+--------+-----+-----+-----+
    // offs12 is given in multiples of two bytes, but as we let it be specified in bytes we have to shift it by 19
    // positions rather than 20, masking off the bottom bit.
    Branch(Opcode opc, uint32_t rs1, uint32_t rs2, uint32_t offs12)
        : v{((offs12 & 0xfffffffe) << 19) | (rs2 << 12) | (rs1 << 7) | uint32_t(opc)}
    {
    }

    operator uint32_t() const { return v; }

    auto opc() const -> Opcode { return Opcode(v & 0x7f); }
    auto rs1() const -> uint32_t { return (v >> 7) & 0x1f; }
    auto rs2() const -> uint32_t { return (v >> 12) & 0x1f; }
    auto sxoffs12() const -> uint32_t { return uint32_t(int32_t(v) >> 19) & 0xfffffffe; }

private:
    uint32_t v;
};

struct CallT
{
    // +--------+----+-----+
    // | offs20 | rd | opc |
    // |     20 |  5 |   7 |
    // +--------+----+-----+
    // offs20 is given in multiples of two bytes, but as we let it be specified in bytes we have to shift it by 11
    // positions rather than 12, masking off the bottom bit.
    CallT(Opcode opc, uint32_t rd, uint32_t offs20) : v{((offs20 & 0xfffffffe) << 11) | (rd << 7) | uint32_t(opc)} {}

    operator uint32_t() const { return v; }

    auto opc() const -> Opcode { return Opcode(v & 0x7f); }
    auto rd() const -> uint32_t { return (v >> 7) & 0x1f; }
    auto sxoffs20() const -> uint32_t { return uint32_t(int32_t(v) >> 11) & 0xfffffffe; }

private:
    uint32_t v;
};

struct Uimm20
{
    // +-------+----+-----+
    // | imm20 | rd | opc |
    // |    20 |  5 |   7 |
    // +-------+----+-----+
    Uimm20(Opcode opc, uint32_t rd, uint32_t imm20) : v{(imm20 << 12) | (rd << 7) | uint32_t(opc)} {}

    operator uint32_t() const { return v; }

    auto opc() const -> Opcode { return Opcode(v & 0x7f); }
    auto rd() const -> uint32_t { return (v >> 7) & 0x1f; }
    auto zximm20() const -> uint32_t { return v & 0xfffff000; }

private:
    uint32_t v;
};

struct Decode
{
    Decode(uint32_t value) : v{value} {}

    Opcode Op() const { return Opcode(v & 0x7f); }

    union
    {
        Reg reg;
        Immediate immediate;
        Branch branch;
        CallT call;
        Uimm20 lui;
        uint32_t v;
    };
};
static_assert(sizeof(Decode) == sizeof(uint32_t));

enum class Mode
{
    run,
    disassemble,
    trace
};

template<Mode mode = Mode::run>
void Run(const std::uint32_t* code)
{
    constexpr bool isTrace = (mode == Mode::disassemble) || (mode == Mode::trace);
    constexpr bool isRun = (mode == Mode::run) || (mode == Mode::trace);

    uint32_t pc = 0;
    uint32_t x[32] = {};
    uint32_t nextPc = 0;

    bool done = false;

    while (!done)
    {
        // Fetch a word from memory.
        pc = nextPc;
        nextPc = pc + sizeof(*code);
        const uint32_t word = code[pc / sizeof(*code)];

        if constexpr (isTrace)
        {
            std::cout << std::format("{:04x}: ", pc);
        }

        // Decode it into an instruction.
        const auto instruction = Decode(word);

        // Dispatch on the instruction's opcode.
        const auto opcode = instruction.Op();
        switch (opcode)
        {
        case Opcode::Add: {
            auto rd = instruction.reg.rd();
            auto rs1 = instruction.reg.rs1();
            auto rs2 = instruction.reg.rs2();
            if constexpr (isTrace)
            {
                std::cout << std::format("add {}, {}, {}\n", regnames[rd], regnames[rs1], regnames[rs2]);
            }
            if constexpr (isRun)
            {
                x[rd] = x[rs1] + x[rs2];
                x[0] = 0;
            }
            break;
        }
        case Opcode::Addi: {
            auto rd = instruction.immediate.rd();
            auto rs = instruction.immediate.rs();
            auto sximm12 = instruction.immediate.sximm12();
            if constexpr (isTrace)
            {
                std::cout << std::format("addi {}, {}, {}\n", regnames[rd], regnames[rs], sximm12);
            }
            if constexpr (isRun)
            {
                x[rd] = x[rs] + sximm12;
                x[0] = 0;
            }
            break;
        }
        case Opcode::Beq: {
            auto rs1 = instruction.branch.rs1();
            auto rs2 = instruction.branch.rs2();
            auto sxoffs12 = instruction.branch.sxoffs12();
            if constexpr (isTrace)
            {
                std::cout << std::format("beq {}, {}, {:04x}\n", regnames[rs1], regnames[rs2], pc + sxoffs12);
            }
            if constexpr (isRun)
            {
                if (x[rs1] == x[rs2])
                {
                    nextPc = pc + sxoffs12;
                }
            }
            break;
        }
        case Opcode::Bltu: {
            auto rs1 = instruction.branch.rs1();
            auto rs2 = instruction.branch.rs2();
            auto sxoffs12 = instruction.branch.sxoffs12();
            if constexpr (isTrace)
            {
                std::cout << std::format("bltu {}, {}, {:04x}\n", regnames[rs1], regnames[rs2], pc + sxoffs12);
            }
            if constexpr (isRun)
            {
                if (x[rs1] < x[rs2])
                {
                    nextPc = pc + sxoffs12;
                }
            }
            break;
        }
        case Opcode::Call: {
            auto sxoffs20 = instruction.call.sxoffs20();
            if constexpr (isTrace)
            {
                std::cout << std::format("call {}\n", pc + sxoffs20);
            }
            if constexpr (isRun)
            {
                x[ra] = pc + 4;
                x[0] = 0;
                // nextPc = pc + sxoffs20;
                std::cout << std::format("fib({}) = {}\n", x[a1], x[a2]);
            }
            break;
        }
        case Opcode::J: {
            auto sxoffs20 = instruction.call.sxoffs20();
            if constexpr (isTrace)
            {
                std::cout << std::format("j {:04x}\n", pc + sxoffs20);
            }
            if constexpr (isRun)
            {
                nextPc = pc + sxoffs20;
            }
            break;
        }
        case Opcode::Li: {
            auto rd = instruction.immediate.rd();
            auto sximm12 = instruction.immediate.sximm12();
            if constexpr (isTrace)
            {
                std::cout << std::format("li {}, {}\n", regnames[rd], pc + sximm12);
            }
            if constexpr (isRun)
            {
                x[rd] = sximm12;
                x[0] = 0;
            }
            break;
        }
        case Opcode::Lui: {
            auto rd = instruction.lui.rd();
            auto zximm20 = instruction.lui.zximm20();
            if constexpr (isTrace)
            {
                std::cout << std::format("lui {}, {}\n", regnames[rd], zximm20);
            }
            if constexpr (isRun)
            {
                x[rd] = zximm20 << 12;
                x[0] = 0;
            }
            break;
        }
        case Opcode::Mv: {
            auto rd = instruction.immediate.rd();
            auto rs = instruction.immediate.rs();
            if constexpr (isTrace)
            {
                std::cout << std::format("mv {}, {}\n", regnames[rd], regnames[rs]);
            }
            if constexpr (isRun)
            {
                x[rd] = x[rs];
                x[0] = 0;
            }
            break;
        }
        default:
            // Illegal instruction.
            if constexpr (isTrace)
            {
                std::cout << std::format("illegal opcode {}\n", uint32_t(opcode));
            }
            done = true;
        }
    }
}

class Assembler
{
    std::vector<uint32_t> code_;

public:
    const std::vector<uint32_t>& Code() const { return code_; }

    void Emit(uint32_t u) { code_.push_back(u); }

    void Li(uint32_t rd, int32_t imm12)
    {
        // li rd, imm12
        Emit(Immediate(Opcode::Li, rd, 0, uint32_t(imm12)));
    }

    void Lui(uint32_t rd, uint32_t uimm20)
    {
        // lui rd, imm20
        Emit(Uimm20(Opcode::Lui, rd, uimm20));
    }

    void Addi(uint32_t rd, uint32_t rs, int32_t imm12)
    {
        // addi rd, rs, imm12
        Emit(Immediate(Opcode::Addi, rd, rs, uint32_t(imm12)));
    }

    void J(int32_t offs20)
    {
        // j offs20
        Emit(CallT(Opcode::J, 0, uint32_t(offs20)));
    }

    void Mv(uint32_t rd, uint32_t rs)
    {
        // mv rd, rs
        Emit(Immediate(Opcode::Mv, rd, rs, 0));
    }

    void Call(int32_t offs20)
    {
        // call offs20
        Emit(CallT(Opcode::Call, ra, uint32_t(offs20)));
    }

    void Beq(uint32_t rs1, uint32_t rs2, int32_t offs12)
    {
        // beq rs1, rs2, offs12
        Emit(Branch(Opcode::Beq, rs1, rs2, uint32_t(offs12)));
    }

    void Bltu(uint32_t rs1, uint32_t rs2, int32_t offs12)
    {
        // bltu rs1, rs2, offs12
        Emit(Branch(Opcode::Bltu, rs1, rs2, uint32_t(offs12)));
    }

    void Add(uint32_t rd, uint32_t rs1, uint32_t rs2)
    {
        // add rd, rs1, rs2
        Emit(Reg(Opcode::Add, rd, rs1, rs2));
    }
};

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

    // clang-format off
// main:
    a.Li(s0, 0);                // 0000: li      s0, 0                   ; i = 0
    a.Li(s2, 2);                // 0004: li      s2, 2                   ; s2 = 2
    a.Lui(a0, 1);               // 0008: lui     a0, %hi(format_str)
    a.Addi(s1, a0, -548);       // 000c: addi    s1, a0, %lo(format_str) ; s1 = the address of the printf format string
    a.Li(s3, 48);               // 0010: li      s3, 48                  ; s3 = 48
    a.Li(s4, 1);                // 0014: li      s4, 1                   ; s4 = 1
    a.J(fib);                   // 0018: j       fib                     ; go to fib
// print_loop:
    a.Mv(a0, s1);               // 001c: mv      a0, s1                  ; arg0 = the address of the printf format string
    a.Mv(a1, s0);               // 0020: mv      a1, s0                  ; arg1 = i (arg2 contains current)
    a.Call(printf);             // 0024: call    printf                  ; call printf
    a.Addi(s0, s0, 1);          // 0028: addi    s0, s0, 1               ; i = i + 1
    a.Beq(s0, s3, done);        // 002c: beq     s0, s3, done            ; if i == 48 go to done
// fib:
    a.Mv(a2, s0);               // 0030: mv      a2, s0                  ; current = i
    a.Bltu(s0, s2, print_loop1);// 0034: bltu    s0, s2, print_loop      ; if i < 2 go to print_loop
    a.Li(a0, 0);                // 0038: li      a0, 0                   ; previous = 0
    a.Li(a2, 1);                // 003c: li      a2, 1                   ; current = 1
    a.Mv(a1, s0);               // 0040: mv      a1, s0                  ; n = i
// fib_loop:
    a.Mv(a3, a2);               // 0044: mv      a3, a2                  ; tmp = current
    a.Addi(a1, a1, -1);         // 0048: addi    a1, a1, -1              ; n = n - 1
    a.Add(a2, a0, a2);          // 004c: add     a2, a0, a2              ; current = current + prev
    a.Mv(a0, a3);               // 0050: mv      a0, a3                  ; previous = tmp
    a.Bltu(s4, a1, fib_loop);   // 0054: bltu    s4, a1, fib_loop        ; if n > 1 go to fib_loop
    a.J(print_loop2);           // 0058: j       print_loop              ; go to print_loop
// done:
    a.Li(a0, 0);                // 005c: li      a0, 0                   ; set the return value of main() to 0
    // clang-format on

    // Emit an illegal instruction so that we have something to stop us.
    a.Emit(0);

    return a.Code();
}

int main()
{
    auto code = Assemble();

    Run<Mode::run>(code.data());
}
