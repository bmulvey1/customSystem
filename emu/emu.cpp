#include <iostream>
#include <cstdint>
#include "names.h"

uint8_t memory[0xffff] = {0x46, 0x00, 0x46, 0x00, 0x40, 0x00};
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
uint16_t registers[9] = {12};

#define readByte(address) memory[address]
#define readWord(address) memory[address] << 8 + memory[address + 1]
#define consumeByte(address) memory[address++]
#define consumeWord(address) memory[address++] << 8 + memory[address++]
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
        switch (opCode)
        {
        case 0x00:
            break;

        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            switch (opCode & 0b1111)
            {
            case 0x0:
                registers[RD] += registers[RS];
                break;
            case 0x1:
                registers[RD] -= registers[RS];
                break;
            case 0x2:
                registers[RD] *= registers[RS];
                break;
            case 0x3:
                registers[RD] /= registers[RS];
                break;
            case 0x4:
                registers[RD] >>= registers[RS];
                break;
            case 0x5:
                registers[RD] <<= registers[RS];
                break;
            }
        }
        break;
        case 0x46:
        case 0x47:
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RD = insByte2 & 0b1111;
            switch (opCode)
            {
            case 0x6:
            printf("inc\n\n");
                registers[RD]+= 1;
                break;
            case 0x7:
                registers[RD]+= 1;
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