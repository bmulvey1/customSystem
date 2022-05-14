#include <iostream>
#include <fstream>
#include <cstdint>
#include "names.hpp"

uint8_t memory[0x10000] = {0};
enum registers
{
    r0,
    r1,
    r2,
    r3,
    r4,
    r5,
    r6,
    r7,
    r8,
    r9,
    ra,
    rb,
    rc,
    rd,
    re,
    rf,
    rr,
    sp,
    bp,
    ip
};
uint16_t registers[32] = {0};

enum flags
{
    NF,
    ZF,
    CF,
    VF,
};
uint8_t flags[4] = {0};

std::string registerNames[] = {
    "%r0", "%r1", "%r2", "%r3", "%r4", "%r5", "%r6", "%r7", "%r8", "%r9", "%ra", "%rb", "%rc", "%rd", "%re", "%rf", "%rr", "%sp", "%bp", "%ip"};

#define readByte(address) memory[address]
#define readWord(address) (memory[address] << 8) + memory[address + 1]
#define consumeByte(address) memory[address++]
#define consumeWord(address)                      \
    (memory[address] << 8) + memory[address + 1]; \
    address += 2

#define writeByte(address, value) memory[address] = value

void printState()
{
    for (int i = 0; i < 20; i++)
    {
        printf("|%4s|", registerNames[i].c_str());
    }
    std::cout << std::endl;
    for (int i = 0; i < 20; i++)
    {
        printf("|%04x|", registers[i]);
    }
    printf("NF: %d ZF: %d CF: %d VF: %d\n", flags[NF], flags[ZF], flags[CF], flags[VF]);

    /*uint32_t stackScan = 0x10000;
    while (stackScan > (uint32_t)registers[sp])
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
    memory[--registers[sp]] = value & 0b11111111;
    memory[--registers[sp]] = value >> 8;
}

uint16_t stackPop()
{
    uint16_t value = memory[registers[sp]++] << 8;
    value |= memory[registers[sp]++];
    return value;
}

void arithmeticOp(uint8_t RD, uint32_t S1, uint32_t S2, uint8_t opCode)
{
    uint32_t result = 0;

    // their compiler optimizes better than mine
    uint16_t invertedS2 = ~S2;
    switch (opCode & 0b1111)
    {
    case 0x0: // 0xX0: ADD
        result = S1 + S2;
        break;
    case 0x1: // 0xX1: SUB
        result = S1 + invertedS2 + 1;
        break;
    case 0x2: // 0xX2: MUL
        result = S1 * S2;
        break;
    case 0x3: // 0xX3: DIV
        result = S1 / S2;
        break;
    case 0x4: // 0xX4: SHR
        result = S1 >> S2;
        break;
    case 0x5: // 0xX5: SHL
        result = S1 << S2;
        break;
    case 0x8: // 0xX8: AND
        result = S1 & S2;
        break;
    case 0x9: // 0xX9: OR
        result = S1 | S2;
        break;
    case 0xa: // 0xXa: XOR
        result = S1 ^ S2;
        break;
    case 0xc: // 0xXc: CMP
        // uint16_t result = registers[RD] - value;
        result = S1 + invertedS2 + 1;
        break;
    }

    flags[ZF] = ((result & 0xffff) == 0);
    flags[NF] = ((result >> 15) & 1);
    flags[CF] = ((result >> 16) > 0);

    flags[VF] = ((registers[RD] >> 15) ^ ((result >> 15) & 1)) && !(1 ^ (registers[RD] >> 15) ^ (S2 >> 15));
    // printf("NF: %d ZF: %d CF: %d VF: %d \n", flags[NF], flags[ZF], flags[CF], flags[VF]);
    // if the two inputs have MSB set
    /*if (registers[RD] >> 15 && value >> 15)
    {
        // opposite of whether result has sign flag
        flags[VF] = !((result >> 15) > 0);
    }
    else
    {
        if (!(registers[RD] >> 15) && !(value >> 15))
        {
            flags[VF] = (result >> 15) > 0;
        }
        else
        {
            flags[VF] = 0;
        }
    }*/

    // set result if not CMP
    if ((opCode & 0b1111) != 0xc)
    {
        if (RD != 0)
            registers[RD] = result;
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
    registers[sp] = 0xffff;
    registers[ip] = readWord(0); // read the entry point to the code segment
    uint8_t opCode;
    int running = 1;
    uint32_t instructionCount = 0;
    while (running)
    {
        opCode = consumeByte(registers[ip]);
        // printf("%4x\t%02x - ", registers[ip], opCode);
        // std::cout << opcodeNames[opCode] << std::endl;
        // printState();
        // for(int i = 0; i < 0xffffff; i++){}
        switch (opCode)
        {
            // hlt/no instruction
        case 0x00:
            running = false;
            break;

        case 0x01: // nop
            break;

        // JMP
        case 0x10:
        {
            uint16_t destination = consumeWord(registers[ip]);
            registers[ip] = destination;
        }
        break;

        // JE/JZ
        case 0x11:
        {
            uint16_t destination = consumeWord(registers[ip]);
            if (flags[ZF])
            {
                registers[ip] = destination;
            }
        }
        break;

        // JNE/JNZ
        case 0x12:
        {
            uint16_t destination = consumeWord(registers[ip]);
            if (flags[ZF] == 0)
            {
                registers[ip] = destination;
            }
        }
        break;

        // JG
        case 0x13:
        {
            uint16_t destination = consumeWord(registers[ip]);
            if ((flags[ZF] == 0) && flags[CF])
            {
                registers[ip] = destination;
            }
        }
        break;

        // JL
        case 0x14:
        {
            uint16_t destination = consumeWord(registers[ip]);
            if (flags[CF] == 0)
            {
                registers[ip] = destination;
            }
        }
        break;

        // JGE
        case 0x15:
        {
            uint16_t destination = consumeWord(registers[ip]);
            if (flags[CF])
            {
                registers[ip] = destination;
            }
        }
        break;

        // JLE
        case 0x16:
        {
            uint16_t destination = consumeWord(registers[ip]);
            if (flags[ZF] || (flags[CF] == 0))
            {
                registers[ip] = destination;
            }
        }
        break;

        // unsigned arithmetic
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
            uint16_t operands = consumeWord(registers[ip]);
            uint8_t RD = operands >> 11;
            uint8_t RS1 = operands >> 6 & 0b11111;
            uint8_t RS2 = operands >> 1 & 0b11111;
            arithmeticOp(RD, registers[RS1], registers[RS2], opCode);
        }
        break;

            // inc/dec/NOT need to handle flags properly
            /*
            case 0x46:
            case 0x47: // INC & DEC
            case 0x4b: // NOT
            {
                uint8_t insByte2 = consumeByte(registers[ip]);
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
            */

        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x58:
        case 0x59:
        case 0x5a:
        // immediate arithmetic
        {
            uint16_t operands = consumeWord(registers[ip]);
            uint8_t RD = operands >> 11;
            uint8_t RS1 = operands >> 6 & 0b11111;
            uint16_t IMM = consumeWord(registers[ip]);
            arithmeticOp(RD, registers[RS1], IMM, opCode);
        }
        break;

        case 0x5c: // cmpi
        {
            uint8_t RS1 = consumeByte(registers[ip]);
            uint16_t IMM = consumeWord(registers[ip]);
            arithmeticOp(0, registers[RS1], IMM, opCode);
        }
        break;

        /*
        // MOVB (move byte)
        case 0xa0: // register -> register MOV
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            registers[RD] = registers[RS] & 0b11111111;
        }
        break;

        case 0xa1: // dereferenced register -> register mov
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            registers[RD] = readByte(registers[RS]);
        }
        break;

        case 0xa2: // register -> dereferenced register mov
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            writeByte(registers[RD], registers[RS] & 0b11111111);
        }
        break;

        // dereferenced register + offset -> register
        case 0xa3:
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            int8_t offset = consumeByte(registers[ip]);
            registers[RD] = readByte(registers[RS] + offset);
        }
        break;

        case 0xa4:
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            int8_t offset = consumeByte(registers[ip]);
            writeByte(registers[RD] + offset, registers[RS] & 0b11111111);
        }
        break;

        case 0xa5: // movb roffset(rd, scale), rd
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            uint8_t RO = consumeByte(registers[ip]);
            uint8_t scale = consumeByte(registers[ip]);
            uint16_t address = registers[RS] + (scale * registers[RO]);
            registers[RD] = readByte(address);
        }
        break;

        case 0xa6: // movb rs, roffset(rd, scale)
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint8_t RS = insByte2 >> 4;
            uint8_t RD = insByte2 & 0b1111;
            uint8_t RO = consumeByte(registers[ip]);
            uint8_t scale = consumeByte(registers[ip]);
            uint16_t address = registers[RD] + (scale * registers[RO]);
            writeByte(address, registers[RS] & 0b11111111);
        }
        break;

        case 0xa7:
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint8_t RD = insByte2 & 0b1111;
            uint8_t imm = consumeByte(registers[ip]);
            registers[RD] = imm;
        }
        break;
        */
        // MOVW (move word)
        case 0xa8:
        {
            uint16_t operands = consumeWord(registers[ip]);
            uint8_t RS = operands >> 11;
            uint8_t RD = operands >> 6 & 0b11111;
            registers[RD] = registers[RS];
        }
        break;

        case 0xa9: // dereferenced register -> register mov
        {
            uint16_t operands = consumeWord(registers[ip]);
            uint8_t RS = operands >> 11;
            uint8_t RD = operands >> 6 & 0b11111;
            registers[RD] = readByte(registers[RS]) << 8;
            registers[RD] |= readByte(registers[RS] + 1);
        }
        break;

        case 0xaa: // register -> dereferenced register mov
        {
            uint16_t operands = consumeWord(registers[ip]);
            uint8_t RS = operands >> 11;
            uint8_t RD = operands >> 6 & 0b11111;
            writeByte(registers[RD], registers[RS] >> 8);
            writeByte(registers[RD] + 1, registers[RS]);
        }
        break;

        // (register + offset) dereferenced -> register
        case 0xab: // MOV
        {
            uint16_t operands = consumeWord(registers[ip]);
            uint8_t RS = operands >> 11;
            uint8_t RD = operands >> 6 & 0b11111;
            int16_t offset = consumeWord(registers[ip]);
            registers[RD] = readByte(registers[RS] + offset) << 8;
            registers[RD] |= readByte(registers[RS] + offset + 1);
        }
        break;

        case 0xac: // MOV
        {
            uint16_t operands = consumeWord(registers[ip]);
            uint8_t RS = operands >> 11;
            uint8_t RD = operands >> 6 & 0b11111;
            int16_t offset = consumeWord(registers[ip]);
            writeByte(registers[RD] + offset, registers[RS] >> 8);
            writeByte(registers[RD] + offset + 1, registers[RS] & 0b11111111);
        }
        break;

        case 0xad: // MOV rd, roffset(rs, scale)
        {
            uint16_t operands = consumeWord(registers[ip]);
            uint8_t RS = operands >> 11;
            uint8_t RD = operands >> 6 & 0b11111;
            uint8_t RO = operands >> 1 & 0b11111;
            uint8_t scale = consumeByte(registers[ip]);
            uint16_t address = registers[RS] + (scale * registers[RO]);
            registers[RD] = readByte(address) << 8;
            registers[RD] |= readByte(address + 1);
        }
        break;

        case 0xae: // MOV roffset(rs, scale), rd
        {
            uint16_t operands = consumeWord(registers[ip]);
            uint8_t RS = operands >> 11;
            uint8_t RD = operands >> 6 & 0b11111;
            uint8_t RO = operands >> 1 & 0b11111;
            uint8_t scale = consumeByte(registers[ip]);

            uint16_t address = registers[RD] + (scale * registers[RO]);
            writeByte(address, registers[RS] >> 8);
            writeByte(address + 1, registers[RS] & 0b11111111);
        }
        break;

        case 0xaf: // MOV imm
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint8_t RD = insByte2 & 0b11111;
            uint16_t imm = consumeWord(registers[ip]);
            registers[RD] = imm;
        }
        break;

        case 0xc0: // PUSH
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            stackPush(registers[insByte2]);
        }
        break;

        case 0xc1: // PUSH
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint16_t value = readWord(registers[insByte2]);
            stackPush(value);
        }
        break;

        case 0xc2: // PUSH
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            int8_t offset = consumeByte(registers[ip]);
            uint16_t value = readWord(registers[insByte2] + offset);
            stackPush(value);
        }
        break;

        case 0xc3: // PUSH
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            uint8_t RS = insByte2 & 0b1111;
            uint8_t RO = insByte2 >> 4;
            uint8_t scale = consumeByte(registers[ip]);
            uint16_t value = readWord(registers[RS] + (registers[RO] * scale));
            stackPush(value);
        }
        break;

        case 0xc4: // PUSH imm
        {
            uint16_t immediate = consumeWord(registers[ip]);
            stackPush(immediate);
        }
        break;

        case 0xcf: // POP
        {
            uint8_t insByte2 = consumeByte(registers[ip]);
            registers[insByte2] = stackPop();
        }
        break;

        case 0xd0: // CALL
        {
            uint16_t callAddr = consumeWord(registers[ip]);
            stackPush(registers[bp]);
            stackPush(registers[ip]);
            registers[bp] = registers[sp];
            registers[ip] = callAddr;
        }
        break;

        case 0xd1: // RET
        {
            registers[ip] = stackPop();
            registers[bp] = stackPop();
        }
        break;

        case 0xd2: // RET {argc}
        {
            uint16_t argw = consumeWord(registers[ip]);
            registers[ip] = stackPop();
            registers[bp] = stackPop();
            registers[sp] += argw;
        }
        break;

        // OUT (bootleg print)
        case 0xd3:
        {
            uint8_t rs = consumeByte(registers[ip]);
            printf("%%r%d: %d\n", rs, registers[rs]);
        }
        break;

        case 0xff:
            running = false;
            break;
        }

        // printf("%4x\t%02x - ", registers[ip], opCode);
        // std::cout << opcodeNames[opCode] << std::endl;
        // printState();
        // for (int i = 0; i < 0xffffff; i++){}

        // printf("\n");
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