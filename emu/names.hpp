#include <string>
#include <map>

#pragma once
std::map<uint8_t, std::string> opcodeNames =
    {
        {0x00, "UND"},
        {0x01, "NOP"},
        {0x11, "JMP"},
        {0x13, "JE/JZ"},
        {0x15, "JNE/JNZ"},
        {0x17, "JG"},
        {0x19, "JL"},
        {0x1b, "JGE"},
        {0x1d, "JLE"},

        {0x41, "ADD"},
        {0x43, "SUB"},
        {0x45, "MUL"},
        {0x47, "DIV"},
        {0x49, "SHR"},
        {0x4b, "SHL"},
        {0x4d, "AND"},
        {0x4f, "OR"},
        {0x51, "XOR"},
        {0x53, "CMP"},
        {0x54, "INC"},
        {0x56, "DEC"},
        {0x58, "NOT"},

        {0x61, "ADDI"},
        {0x63, "SUBI"},
        {0x65, "MULI"},
        {0x67, "DIVI"},
        {0x69, "SHRI"},
        {0x6b, "SHLI"},
        {0x6d, "ANDI"},
        {0x6f, "ORI"},
        {0x71, "XORI"},
        {0x73, "CMPI"},

        {0xa0, "MOVB reg, reg"},
        {0xa2, "MOVB reg, (reg)"},
        {0xa4, "MOVB (reg), reg"},
        {0xa5, "MOVB reg, (reg+off)"},
        {0xa7, "MOVB (reg + off), reg"},
        {0xa9, "MOVB reg, (reg+reg*sclpow)"},
        {0xab, "MOVB (reg+reg*sclpow), reg"},
        {0xaf, "MOVB reg, imm"},

        {0xb0, "MOVH reg, reg"},
        {0xb2, "MOVH reg, (reg)"},
        {0xb4, "MOVH (reg), reg"},
        {0xb5, "MOVH reg, (reg+off)"},
        {0xb7, "MOVH (reg + off), reg"},
        {0xb9, "MOVH reg, (reg+reg*sclpow)"},
        {0xbb, "MOVH (reg+reg*sclpow), reg"},
        {0xbf, "MOVH reg, imm"},

        {0xc0, "MOV reg, reg"},
        {0xc2, "MOV reg, (reg)"},
        {0xc4, "MOV (reg), reg"},
        {0xc5, "MOV reg, (reg+off)"},
        {0xc7, "MOV (reg + off), reg"},
        {0xc9, "MOV reg, (reg+reg*sclpow)"},
        {0xcb, "MOV (reg+reg*sclpow), reg"},

        {0xd0, "PUSH"},
        {0xd1, "PUSHI"},
        {0xd2, "POP"},    

        {0xd3, "CALL"},
        {0xd4, "RET"},
        {0xd5, "INT"},
        {0xd6, "RETI"},

        {0xe2, "OUT"},
        {0xfe, "HLT"}
};