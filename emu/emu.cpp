#include <iostream>
#include <cstdint>
#include "names.h"

uint8_t memory[0xffff] = {0x48, 0x01, 0x12, 0x34};
enum registerNames
{
    RA,
    RB,
    RC,
    RD,
    RSI,
    RDI,
    RSP,
    RBP,
    IP
};
uint16_t registers[9] = {0};

#define readByte(address) memory[address]
#define readWord(address) memory[address] << 8 + memory[address + 1]
#define consumeByte(address) memory[address++]
#define consumeWord(address) (memory[address] << 8) + memory[address + 1]; address += 2

void printState(){
    std::cout << " RA  RB  RC  RD  RSI RDI RBP RSP" << std::endl;
    for(int i = 0; i < 8; i++){
        printf("[%4x]", registers[i]);
    }
    std::cout << std::endl;
}

int main()
{
    uint8_t opCode;
    int running = 1;
    while (running)
    {
        opCode = consumeByte(registers[IP]);
        std::cout << opcodeNames[opCode] << std::endl;
        printf("%02x\n", opCode);
        switch (opCode)
        {
        case 0x00:
            break;

        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45: // simple arithmetic
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            switch (opCode & 0b1111)
            {
            case 0x0: // 0x40: ADD
                registers[RD] += registers[RS];
                break;
            case 0x1: // 0x41: SUB
                registers[RD] -= registers[RS];
                break;
            case 0x2: // 0x42: MUL
                registers[RD] *= registers[RS];
                break;
            case 0x3: // 0x43: DIV
                registers[RD] /= registers[RS];
                break;
            case 0x4: // 0x44: SHR 
                registers[RD] >>= registers[RS];
                break;
            case 0x5: // 0x45: SHL
                registers[RD] <<= registers[RS];
                break;
            }
        }
        break;
        case 0x46:
        case 0x47: // INC & DEC
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RD = insByte2 & 0b1111;
            switch (opCode & 0b1111)
            {
            case 0x6: // 0x46: INC
                registers[RD]+= 1;
                break;
            case 0x7: // 0x47: DEC
                registers[RD]-= 1;
                break;
            }
        }
        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
        case 0x4c:
        case 0x4d: // immediate arithmetic
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RD = insByte2 & 0b1111;
            uint16_t imm = consumeWord(registers[IP]);
            switch (opCode & 0b111)
            {
            case 0x0: // 0x48: ADDI
                registers[RD] += imm;
                break;
            case 0x1: // 0x49: SUBI
                registers[RD] -= imm;
                break;
            case 0x2: // 0x4a: MULI
                registers[RD] *= imm;
                break;
            case 0x3: // 0x4b: DIVI
                registers[RD] /= imm;
                break;
            case 0x4: // 0x4c: SHR I
                registers[RD] >>= imm;
                break;
            case 0x5: // 0x4d: SHLI
                registers[RD] <<= imm;
                break;
            }
        }
        break;
        }

        printState();
        for (int i = 0; i < 0xfffffff; i++)
        {
        }
    }
}