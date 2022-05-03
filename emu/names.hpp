#include <string>
#pragma once
std::string opcodeNames[256] =
    {
        // Control Flow (0x00)
        "NOP",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        // 0x08
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        // 0x10
        "JMP",
        "JE/JZ",
        "JNE/JNZ",
        "JG",
        "JL",
        "JGE",
        "JLE",
        "UND",
        // 0x18
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        // 0x20
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        //
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        // 0x30
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        //
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        // Arithmetic 0x40
        // Simple
        "ADD reg <-= reg",
        "SUB reg <-= reg",
        "MUL reg <-= reg",
        "DIV reg <-= reg",
        "SHR reg <-= reg",
        "SHL reg <-= reg",
        "INC reg <-= reg",
        "DEC reg <-= reg",
        //
        "AND reg <-= reg",
        "OR  reg <-= reg",
        "XOR reg <-= reg",
        "NOT reg <-= reg",
        "CMP reg <-= reg",
        "UND",
        "UND",
        "UND",
        // Arithmetic 0x50
        // register indirect
        "ADD (reg) <-= reg",
        "SUB (reg) <-= reg",
        "MUL (reg) <-= reg",
        "DIV (reg) <-= reg",
        "SHR (reg) <-= reg",
        "SHL (reg) <-= reg",
        "INC (reg) <-= reg",
        "DEC (reg) <-= reg",
        //
        "AND (reg) <-= reg",
        "OR  (reg) <-= reg",
        "XOR (reg) <-= reg",
        "UND",
        "CMP (reg) <-= reg",
        "UND",
        "UND",
        "UND",
        // Arithmetic 0x60
        // register indirect with offset
        "ADD off8(reg) <-= reg",
        "SUB off8(reg) <-= reg",
        "MUL off8(reg) <-= reg",
        "DIV off8(reg) <-= reg",
        "SHR off8(reg) <-= reg",
        "SHL off8(reg) <-= reg",
        "INC off8(reg) <-= reg",
        "DEC off8(reg) <-= reg",
        //
        "AND off8(reg) <-= reg",
        "OR  off8(reg) <-= reg",
        "XOR off8(reg) <-= reg",
        "UND",
        "CMP off8(reg) <-= reg",
        "UND",
        "UND",
        "UND",
        //
        // Arithmetic (0x70)
        // register indirect with scale and offset
        "ADD offreg(reg, scale) <-= reg",
        "SUB offreg(reg, scale) <-= reg",
        "MUL offreg(reg, scale) <-= reg",
        "DIV offreg(reg, scale) <-= reg",
        "SHR offreg(reg, scale) <-= reg",
        "SHL offreg(reg, scale) <-= reg",
        "INC offreg(reg, scale) <-= reg",
        "DEC offreg(reg, scale) <-= reg",
        //
        "AND offreg(reg, scale) <-= reg",
        "OR  offreg(reg, scale) <-= reg",
        "XOR offreg(reg, scale) <-= reg",
        "UND",
        "CMP offreg(reg, scale) <-= reg",
        "UND",
        "UND",
        "UND",
        //
        // Arithmetic (0x80)
        // immediate
        "ADD imm16 <-= reg",
        "SUB imm16 <-= reg",
        "MUL imm16 <-= reg",
        "DIV imm16 <-= reg",
        "SHR imm16 <-= reg",
        "SHL imm16 <-= reg",
        "INC imm16 <-= reg",
        "DEC imm16 <-= reg",
        //
        "AND imm16 <-= reg",
        "OR  imm16 <-= reg",
        "XOR imm16 <-= reg",
        "UND",
        "CMP im16 <-= reg",
        "UND",
        "UND",
        "UND",
        // 0x90
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        //
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        //
        // Data control (byte) 0xa0
        //
        "MOVB reg <- reg",
        "MOVB reg <- (reg)",
        "MOVB (reg) <- reg",
        "MOVB reg <- off8(reg)",
        "MOVB off8(reg) <- reg",
        "MOVB reg <- offreg(basereg, scale)",
        "MOVB offreg(basereg, scale) <- reg",
        "MOVB reg <- imm8",
        //
        "MOVW reg <- reg",
        "MOVW reg <- (reg)",
        "MOVW (reg) <- reg",
        "MOVW reg <- off8(reg)",
        "MOVW off8(reg) <- reg",
        "MOVW reg <- offreg(basereg, scale)",
        "MOVW offreg(basereg, scale) <- reg",
        "MOVW reg <- imm16",
        // 0xb0
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        //
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        //
        // stack manipulation 0xc0
        "PUSH reg",
        "PUSH (reg)",
        "PUSH off(reg)",
        "PUSH offreg(basereg, scale)",
        "PUSH imm16",
        "UND",
        "UND",
        "UND",
        //
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "POP reg",
        // 0xd0
        "CALL",
        "RET",
        "RETC",
        "OUT",
        "UND",
        "UND",
        "UND",
        "UND",
        //
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        // 0xe0
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        //
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        // 0xf0
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        //
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "UND",
        "HLT"};