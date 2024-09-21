#pragma once

#include <cstdint>

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
