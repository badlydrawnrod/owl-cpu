#pragma once

#include <concepts>
#include <cstdint>

template<typename T>
concept InstructionHandler =
        requires(T t, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t shift, int32_t offs12,
                 int32_t imm12, int32_t offs20, uint32_t uimm20, uint32_t ins) {
            //
            typename T::Item;
            //
            { t.Ecall() } -> std::same_as<typename T::Item>;
            { t.Ebreak() } -> std::same_as<typename T::Item>;
            //
            { t.Add(r0, r1, r2) } -> std::same_as<typename T::Item>;
            { t.Sub(r0, r1, r2) } -> std::same_as<typename T::Item>;
            { t.Sll(r0, r1, r2) } -> std::same_as<typename T::Item>;
            { t.Slt(r0, r1, r2) } -> std::same_as<typename T::Item>;
            { t.Sltu(r0, r1, r2) } -> std::same_as<typename T::Item>;
            { t.Xor(r0, r1, r2) } -> std::same_as<typename T::Item>;
            { t.Srl(r0, r1, r2) } -> std::same_as<typename T::Item>;
            { t.Sra(r0, r1, r2) } -> std::same_as<typename T::Item>;
            { t.Or(r0, r1, r2) } -> std::same_as<typename T::Item>;
            { t.And(r0, r1, r2) } -> std::same_as<typename T::Item>;
            //
            { t.Slli(r0, r1, shift) } -> std::same_as<typename T::Item>;
            { t.Srli(r0, r1, shift) } -> std::same_as<typename T::Item>;
            { t.Srai(r0, r1, shift) } -> std::same_as<typename T::Item>;
            //
            { t.Beq(r0, r1, offs12) } -> std::same_as<typename T::Item>;
            { t.Bne(r0, r1, offs12) } -> std::same_as<typename T::Item>;
            { t.Blt(r0, r1, offs12) } -> std::same_as<typename T::Item>;
            { t.Bge(r0, r1, offs12) } -> std::same_as<typename T::Item>;
            { t.Bltu(r0, r1, offs12) } -> std::same_as<typename T::Item>;
            { t.Bgeu(r0, r1, offs12) } -> std::same_as<typename T::Item>;
            //
            { t.Addi(r0, r1, imm12) } -> std::same_as<typename T::Item>;
            { t.Slti(r0, r1, imm12) } -> std::same_as<typename T::Item>;
            { t.Sltiu(r0, r1, imm12) } -> std::same_as<typename T::Item>;
            { t.Xori(r0, r1, imm12) } -> std::same_as<typename T::Item>;
            { t.Ori(r0, r1, imm12) } -> std::same_as<typename T::Item>;
            { t.Andi(r0, r1, imm12) } -> std::same_as<typename T::Item>;
            //
            { t.Lb(r0, imm12, r1) } -> std::same_as<typename T::Item>;
            { t.Lbu(r0, imm12, r1) } -> std::same_as<typename T::Item>;
            { t.Lh(r0, imm12, r1) } -> std::same_as<typename T::Item>;
            { t.Lhu(r0, imm12, r1) } -> std::same_as<typename T::Item>;
            { t.Lw(r0, imm12, r1) } -> std::same_as<typename T::Item>;
            //
            { t.Sb(r0, imm12, r1) } -> std::same_as<typename T::Item>;
            { t.Sh(r0, imm12, r1) } -> std::same_as<typename T::Item>;
            { t.Sw(r0, imm12, r1) } -> std::same_as<typename T::Item>;
            //
            { t.Fence() } -> std::same_as<typename T::Item>;
            //
            { t.Jalr(r0, offs12, r1) } -> std::same_as<typename T::Item>;
            { t.Jal(r0, offs20) } -> std::same_as<typename T::Item>;
            //
            { t.Lui(r0, uimm20) } -> std::same_as<typename T::Item>;
            { t.Auipc(r0, uimm20) } -> std::same_as<typename T::Item>;
            //
            { t.J(offs20) } -> std::same_as<typename T::Item>;
            { t.Call(offs20) } -> std::same_as<typename T::Item>;
            { t.Ret() } -> std::same_as<typename T::Item>;
            { t.Li(r0, imm12) } -> std::same_as<typename T::Item>;
            { t.Mv(r0, r1) } -> std::same_as<typename T::Item>;
            //
            { t.Illegal(ins) } -> std::same_as<typename T::Item>;
        };
