#include <iostream>
#include <fstream>
#include <cstdint>
#include "names.hpp"

uint8_t memory[0x10000] = {0};
enum registers
{
    RA,
    RB,
    RC,
    RD,
    R0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    SI,
    DI,
    SP,
    BP,
    IP
};
uint16_t registers[17] = {0};

enum flags
{
    ZF,
    SF
};
uint8_t flags[2] = {0};

std::string registerNames[] = {
    "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "SI", "DI", "SP", "BP", "IP"};

#define readByte(address) memory[address]
#define readWord(address) (memory[address] << 8) + memory[address + 1]
#define consumeByte(address) memory[address++]
#define consumeWord(address)                      \
    (memory[address] << 8) + memory[address + 1]; \
    address += 2

#define writeByte(address, value) memory[address] = value

void printState()
{
    for (int i = 0; i < 17; i++)
    {
        printf("|%4s|", registerNames[i].c_str());
    }
    std::cout << std::endl;
    for (int i = 0; i < 17; i++)
    {
        printf("|%04x|", registers[i]);
    }
    printf("\n");

    /*uint32_t stackScan = 0x10000;
    while (stackScan > (uint32_t)registers[SP])
    {
        uint16_t val = readWord(stackScan - 1);
        printf("%04x: %05d // %04x\n", stackScan - 1, val, val);
        stackScan -= 2;
    }
    std::cout << std::endl;
    */
}

void setupMemory(char *filePath)
{
    std::ifstream inFile;
    inFile.open(filePath, std::ifstream::in);
    if (!inFile.good())
    {
        std::cout << "Please enter valid bin file" << std::endl;
        exit(1);
    }
    int i = 0;
    char in;
    while (inFile.good())
    {
        inFile.read(&in, 1);
        writeByte(i++, in);
    }
    inFile.close();
}

void stackPush(int16_t value)
{
    memory[--registers[SP]] = value & 0b11111111;
    memory[--registers[SP]] = value >> 8;
}

uint16_t stackPop()
{
    uint16_t value = memory[registers[SP]++] << 8;
    value |= memory[registers[SP]++];
    return value;
}

void arithmeticOp(uint8_t RD, uint16_t value, uint8_t opCode)
{
    switch (opCode & 0b1111)
    {
    case 0x0: // 0xX0: ADD
        registers[RD] += value;
        break;
    case 0x1: // 0xX1: SUB
        registers[RD] -= value;
        break;
    case 0x2: // 0xX2: MUL
        registers[RD] *= value;
        break;
    case 0x3: // 0xX3: DIV
        registers[RD] /= value;
        break;
    case 0x4: // 0xX4: SHR
        registers[RD] >>= value;
        break;
    case 0x5: // 0xX5: SHL
        registers[RD] <<= value;
        break;
    case 0x8: // 0xX8: AND
        registers[RD] &= value;
        break;
    case 0x9: // 0xX9: OR
        registers[RD] |= value;
        break;
    case 0xa: // 0xXa: XOR
        registers[RD] ^= value;
        break;
    case 0xc: // 0xXc: CMP
        uint16_t result = registers[RD] - value;
        flags[ZF] = (result == 0);
        flags[SF] = (value < registers[RD]);
        break;
    }
    // set the flags based on the result if the instruction isn't CMP
    if ((opCode & 0b1111) != 0xc)
    {
        flags[ZF] = (registers[RD] == 0);
        flags[SF] = (registers[RD] >> 15);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 1)
    {
        std::cout << "Please provide bin file to read asm from!" << std::endl;
        exit(1);
    }
    setupMemory(argv[1]);
    registers[SP] = 0xffff;
    registers[IP] = readWord(0); // read the entry point to the code segment
    uint8_t opCode;
    int running = 1;
    uint32_t instructionCount = 0;
    while (running)
    {
        opCode = consumeByte(registers[IP]);
        printf("%02x\n", opCode);
        std::cout << opcodeNames[opCode] << std::endl;

        switch (opCode)
        {
        case 0x00:
            break;

        case 0x10:
        {
            uint16_t destination = consumeWord(registers[IP]);
            registers[IP] = destination;
        }
        break;

        case 0x11:
        {
            uint16_t destination = consumeWord(registers[IP]);
            if (flags[ZF])
            {
                registers[IP] = destination;
            }
        }
        break;

        case 0x12:
        {
            uint16_t destination = consumeWord(registers[IP]);
            if (!flags[ZF])
            {
                registers[IP] = destination;
            }
        }
        break;

        case 0x13:
        {
            uint16_t destination = consumeWord(registers[IP]);
            if ((flags[ZF] == 0) && (flags[SF] == 0))
            {
                registers[IP] = destination;
            }
        }
        break;

        case 0x14:
        {
            uint16_t destination = consumeWord(registers[IP]);
            if ((flags[ZF] == 0) && (flags[SF] == 1))
            {
                registers[IP] = destination;
            }
        }
        break;

        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4c: // simple arithmetic
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            arithmeticOp(RD, registers[RS], opCode);
        }
        break;

        case 0x46:
        case 0x47: // INC & DEC
        case 0x4b: // NOT
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RD = insByte2 & 0b1111;
            switch (opCode & 0b1111)
            {
            case 0x6: // 0x46: INC
                registers[RD]++;
                break;
            case 0x7: // 0x47: DEC
                registers[RD]--;
                break;
            case 0xb: // 0x48: NOT
                registers[RD] = ~registers[RD];
            }
            flags[ZF] = (registers[RD] == 0);
            flags[ZF] = (registers[RD] >> 15);
        }
        break;

        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5c: // register indirect arithmetic
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RD = insByte2 & 0b1111;
            uint8_t RS = insByte2 >> 4;
            uint16_t value = readWord(registers[RS]);
            arithmeticOp(RD, value, opCode);
        }
        break;

        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x68:
        case 0x69:
        case 0x6a:
        case 0x6c: // register indirect arithmetic with offset
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RD = insByte2 & 0b1111;
            uint8_t RS = insByte2 >> 4;
            int8_t offset = consumeByte(registers[IP]);
            uint16_t value = readWord(registers[RS] + offset);
            arithmeticOp(RD, value, opCode);
        }
        break;

        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x78:
        case 0x79:
        case 0x7a:
        case 0x7c: // register indirect arithmetic with scale
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RD = insByte2 & 0b1111;
            uint8_t RS = insByte2 >> 4;
            uint8_t RO = consumeByte(registers[IP]);
            uint8_t scale = consumeByte(registers[IP]);
            uint16_t value = readWord(registers[RS] + (registers[RO] * scale));
            arithmeticOp(RD, value, opCode);
        }
        break;

        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x88:
        case 0x89:
        case 0x8a:
        case 0x8c: // immediate arithmetic
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RD = insByte2 & 0b1111;
            uint16_t immediate = consumeWord(registers[IP]);
            arithmeticOp(RD, immediate, opCode);
        }
        break;

        // MOVW (move word)
        case 0xa0: // register -> register MOV
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            registers[RD] = registers[RS] & 0b11111111;
        }
        break;

        case 0xa1: // dereferenced register -> register mov
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            registers[RD] = readByte(registers[RS]);
        }
        break;

        case 0xa2: // register -> dereferenced register mov
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            writeByte(registers[RD], registers[RS] & 0b11111111);
        }
        break;

        // dereferenced register + offset -> register
        case 0xa3:
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            int8_t offset = consumeByte(registers[IP]);
            registers[RD] = readByte(registers[RS] + offset);
        }
        break;

        case 0xa4:
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            int8_t offset = consumeByte(registers[IP]);
            writeByte(registers[RD] + offset, registers[RS] & 0b11111111);
        }
        break;

        case 0xa5: // movb roffset(rd, scale), rd
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            uint8_t RO = consumeByte(registers[IP]);
            uint8_t scale = consumeByte(registers[IP]);
            uint16_t address = registers[RS] + (scale * registers[RO]);
            registers[RD] = readByte(address);
        }
        break;

        case 0xa6: // movb rs, roffset(rd, scale)
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            uint8_t RO = consumeByte(registers[IP]);
            uint8_t scale = consumeByte(registers[IP]);
            uint16_t address = registers[RD] + (scale * registers[RO]);
            writeByte(address, registers[RS] & 0b11111111);
        }
        break;

        case 0xa7:
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RD = insByte2 & 0b1111;
            uint8_t imm = consumeByte(registers[IP]);
            registers[RD] = imm;
        }
        break;

        // MOVW (move word)
        case 0xa8:
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            registers[RD] = registers[RS];
        }
        break;

        case 0xa9: // dereferenced register -> register mov
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            registers[RD] = readByte(registers[RS]) << 8;
            registers[RD] |= readByte(registers[RS] + 1);
        }
        break;

        case 0xaa: // register -> dereferenced register mov
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            writeByte(registers[RD], registers[RS] >> 8);
            writeByte(registers[RD] + 1, registers[RS]);
        }
        break;

        // (register + offset) dereferenced -> register
        case 0xab: // MOV
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            int8_t offset = consumeByte(registers[IP]);
            registers[RD] = readByte(registers[RS] + offset) << 8;
            registers[RD] |= readByte(registers[RS] + offset + 1);
        }
        break;

        case 0xac: // MOV
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            int8_t offset = consumeByte(registers[IP]);
            writeByte(registers[RD] + offset, registers[RS] >> 8);
            writeByte(registers[RD] + offset + 1, registers[RS] & 0b11111111);
        }
        break;

        case 0xad: // MOV roffset(rs, scale), rd
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            uint8_t RO = consumeByte(registers[IP]);
            uint8_t scale = consumeByte(registers[IP]);
            uint16_t address = registers[RS] + (scale * registers[RO]);
            registers[RD] = readByte(address) << 8;
            registers[RD] |= readByte(address + 1);
        }
        break;

        case 0xae: // MOV rd, roffset(rs, scale)
        {
            /*
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            uint8_t RO = consumeByte(registers[IP]);
            uint8_t scale = consumeByte(registers[IP]);
            uint16_t address = registers[RD] + (scale * registers[RO]);
            */
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            uint8_t RO = consumeByte(registers[IP]);
            uint8_t scale = consumeByte(registers[IP]);
            uint16_t address = registers[RD] + (scale * registers[RO]);
            writeByte(address, registers[RS] >> 8);
            writeByte(address + 1, registers[RS] & 0b11111111);
        }
        break;

        case 0xaf: // MOV imm
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RD = insByte2 & 0b1111;
            uint16_t imm = consumeWord(registers[IP]);
            registers[RD] = imm;
        }
        break;

        case 0xc0: // PUSH
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            stackPush(registers[insByte2]);
        }
        break;

        case 0xc1: // PUSH
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint16_t value = readWord(registers[insByte2]);
            stackPush(value);
        }
        break;

        case 0xc2: // PUSH
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            int8_t offset = consumeByte(registers[IP]);
            uint16_t value = readWord(registers[insByte2] + offset);
            stackPush(value);
        }
        break;

        case 0xc3: // PUSH
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            uint8_t RS = insByte2 & 0b1111;
            uint8_t RO = insByte2 >> 4;
            uint8_t scale = consumeByte(registers[IP]);
            uint16_t value = readWord(registers[RS] + (registers[RO] * scale));
            stackPush(value);
        }
        break;

        case 0xc4: // PUSH imm
        {
            uint16_t immediate = consumeWord(registers[IP]);
            stackPush(immediate);
        }
        break;

        case 0xcf: // POP
        {
            uint8_t insByte2 = consumeByte(registers[IP]);
            registers[insByte2] = stackPop();
        }
        break;

        case 0xd0: // CALL
        {
            uint16_t callAddr = consumeWord(registers[IP]);
            stackPush(registers[BP]);
            stackPush(registers[IP]);
            registers[BP] = registers[SP];
            registers[IP] = callAddr;
        }
        break;

        case 0xd1: // RET
        {
            registers[IP] = stackPop();
            registers[BP] = stackPop();
        }
        break;

        case 0xff:
            running = false;
            break;
        }
        //printState();
        for (int i = 0; i < 0xffffff; i++)
        {
        }
        //printf("\n");
        instructionCount++;
    }
    printState();
    std::cout << "Execution halted after " << instructionCount << " instructions" << std::endl;
    std::cout << "opening dump file" << std::endl;
    std::ofstream dumpFile;
    dumpFile.open("memdump.bin", std::ofstream::out);
    std::cout << "dump file opened" << std::endl;
    for (char c : memory)
    {
        dumpFile.put(c);
    }
    dumpFile.close();
}