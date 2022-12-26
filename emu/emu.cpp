#include <iostream>
#include <fstream>
#include <cstdint>
#include <chrono>
#include <thread>

#include "names.hpp"
#include "memory.h"

#define PRINTEXECUTION

SystemMemory memory;
// uint8_t memory[0x10000] = {0};
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
    r10,
    r11,
    r12,
    r13,
    sp,
    bp,
    ip
};

uint32_t registers[17] = {0};

enum flags
{
    NF,
    ZF,
    CF,
    VF,
};
uint8_t flags[4] = {0};

std::string registerNames[17] = {
    "%r0", "%r1", "%r2", "%r3", "%r4", "%r5", "%r6", "%r7", "%r8", "%r9", "%ra", "%rb", "%rc", "%rr", "%%sp", "%bp", "%%ip"};

#define readB(address) memory.ReadByte(address);
#define readH(address) ((memory.ReadByte(address) << 8) + memory.ReadByte(address + 1))
#define readW(address) ((memory.ReadByte(address) << 24) + (memory.ReadByte(address + 1) << 16) + (memory.ReadByte(address + 2) << 8) + memory.ReadByte(address + 3))
#define consumeB(address) \
    readB(address);       \
    address++
#define consumeH(address) \
    readH(address);       \
    address += 2
#define consumeW(address) \
    readW(address);       \
    address += 4

#define writeB(address, value) memory.WriteByte(address, value)
#define writeH(address, value)             \
    memory.WriteByte(address, value >> 8); \
    memory.WriteByte(address + 1, value)
#define writeW(address, value)                    \
    memory.WriteByte(address + 0, (value >> 24)); \
    memory.WriteByte(address + 1, (value >> 16)); \
    memory.WriteByte(address + 2, (value >> 8));  \
    memory.WriteByte(address + 3, value)

union instructionData
{
    struct Byte
    {
        uint8_t b3;
        uint8_t b2;
        uint8_t b1;
        uint8_t b0;
    };

    struct Hword
    {
        uint16_t h1;
        uint16_t h0;
    };

    Byte byte;
    Hword hword;
    uint32_t word;
};

void printState()
{
    for (int i = 0; i < 17; i++)
    {
        printf("%8s|", registerNames[i].c_str());
    }
    std::cout << std::endl;
    for (int i = 0; i < 17; i++)
    {
        printf("%8x|", registers[i]);
    }
    printf("\nNF: %d ZF: %d CF: %d VF: %d\n", flags[NF], flags[ZF], flags[CF], flags[VF]);

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
        writeB(i++, in);
    }
    inFile.close();
}

void stackPush(uint32_t value)
{
    registers[sp] -= 4;
    writeW(registers[sp], value);
}

uint32_t stackPop()
{
    uint32_t value = readW(registers[sp]);
    registers[sp] += 4;
    return value;
}

void jmpOp(uint32_t offset24bit)
{
    static const uint64_t mask = 1U << (23);
    uint32_t relativeAddress = ((offset24bit ^ mask) - mask) << 2;
    // printf("Jmp by %d (raw offset was %u)\n", relativeAddress, offset24bit);
    registers[ip] += relativeAddress;
}

void arithmeticOp(uint8_t RD, uint32_t S1, uint32_t S2, uint8_t opCode)
{
    // printf("Arithmetic op with %d %d\n", S1, S2);
    uint64_t result = 0UL;

    // their compiler optimizes better than mine
    uint32_t invertedS2 = ~S2;
    switch (opCode & 0b11111)
    {
    case 0x1: // ADD
        result = S1 + S2;
        flags[CF] = (result < S1);
        break;
    case 0x3: // SUB
        result = S1 + invertedS2 + 1;
        flags[CF] = !(result > S1);
        break;
    case 0x5: // MUL
        result = S1 * S2;
        break;
    case 0x7: // DIV
        result = S1 / S2;
        break;
    case 0x9: // SHR
        result = S1 >> S2;
        break;
    case 0xb: // SHL
        result = S1 << S2;
        break;
    case 0xd: // AND
        result = S1 & S2;
        break;
    case 0xf: // OR
        result = S1 | S2;
        break;
    case 0x11: // XOR
        result = S1 ^ S2;
        break;
    case 0x13: // CMP
        // uint16_t result = registers[RD] - value;
        result = S1 + invertedS2 + 1;
        flags[CF] = !(result > S1);
        break;
    }

    flags[ZF] = ((result & 0xffffffff) == 0);
    flags[NF] = ((result >> 31) & 1);
    // flags[CF] = ((result >> 32) > 0);

    // this seems wrong... is it?
    flags[VF] = ((registers[RD] >> 31) ^ ((result >> 31) & 1)) && !(1 ^ (registers[RD] >> 31) ^ (S2 >> 31));

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
    if ((opCode & 0b11111) != 0x13)
    {
        // if (RD != 0)
        registers[RD] = result;
    }
}

void movOp(instructionData instruction, int nBytes)
{

    switch (instruction.byte.b0 & 0b1111)
    {
        // mov reg, reg
    case 0x0:
    {
        uint8_t RS = instruction.byte.b1 >> 4;
        uint8_t RD = instruction.byte.b1 & 0b1111;
        uint32_t value;
        switch (nBytes)
        {
        case 1:
            value = registers[RS] & 0xffffff00;
            break;
        case 2:
            value = registers[RS] & 0xffff0000;
            break;
        case 4:
            value = registers[RS] & 0x00000000;
            break;
        default:
            printf("Illegal nBytes argument to movOp!\n");
            exit(1);
        };
        registers[RD] = value;
    }
    break;

    // mov reg (reg)
    case 0x2:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
        uint8_t rBase = instruction.byte.b1 & 0b1111;

        switch (nBytes)
        {
        case 1:
            // registers[RD] &= 0xffffff00;
            registers[RD] = readB(rBase);
            break;
        case 2:
            // registers[RD] &= 0xffff0000;
            registers[RD] = readH(rBase);
            break;
        case 4:
            // registers[RD] &= 0x00000000;
            registers[RD] = readW(rBase);
            break;
        default:
            printf("Illegal nBytes argument to movOp!\n");
            exit(1);
        };
    }
    break;

    // mov (reg), reg
    case 0x4:
    {
        uint8_t rBase = instruction.byte.b1 >> 4;
        uint8_t rs = instruction.byte.b1 & 0b1111;

        switch (nBytes)
        {
        case 1:
            writeB(registers[rBase], registers[rs]);
            break;
        case 2:
            writeH(registers[rBase], registers[rs]);
            break;
        case 4:
            writeW(registers[rBase], registers[rs]);
            break;
        default:
            printf("Illegal nBytes argument to movOp!\n");
            exit(1);
        };
    }
    break;

    // mov reg, (reg + off)
    case 0x5:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
        uint8_t rBase = instruction.byte.b1 & 0b1111;
        int16_t offset = instruction.hword.h1;

        int64_t longAddress = registers[rBase];
        longAddress += offset;
        uint32_t address = longAddress;

        // printf("address %08x + %d = %08x\n", registers[rBase], offset, address);

        switch (nBytes)
        {
        case 1:
            registers[RD] = readB(address);
            break;
        case 2:
            registers[RD] = readH(address);
            break;
        case 4:
            registers[RD] = readW(address);
            break;
        default:
            printf("Illegal nBytes argument to movOp!\n");
            exit(1);
        };
    }
    break;

    // mov (reg + off * sclpow), reg
    case 0x7:
    {
        uint8_t RS = instruction.byte.b1 >> 4;
        uint8_t rBase = instruction.byte.b1 & 0b1111;
        int16_t offset = instruction.hword.h1;

        int64_t longaddress = registers[rBase] + offset;
        int32_t address = longaddress;

        // printf("address %08x + %d = %08x\n", registers[rBase], offset, address);

        switch (nBytes)
        {
        case 1:
            writeB(address, registers[RS]);
            break;
        case 2:
            writeH(address, registers[RS]);
            break;
        case 4:
            writeW(address, registers[RS]);
            break;
        default:
            printf("Illegal nBytes argument to movOp!\n");
            exit(1);
        };
    }
    break;

    case 0x9:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
        uint8_t rBase = instruction.byte.b1 & 0b1111;
        uint8_t sclPow = instruction.byte.b2 >> 4;
        int16_t offset = ((instruction.byte.b2 & 0b1111) << 8) | instruction.byte.b3;

        int64_t longaddress = registers[rBase];
        longaddress += (offset << sclPow);
        uint32_t address = longaddress;

        // printf("address %08x + (%d * (2 ^ %d)) = %08x\n", registers[rBase], offset, sclPow, address);

        switch (nBytes)
        {
        case 1:
            registers[RD] = readB(address);
            break;
        case 2:
            registers[RD] = readH(address);
            break;
        case 4:
            registers[RD] = readW(address);
            break;
        default:
            printf("Illegal nBytes argument to movOp!\n");
            exit(1);
        };
    }
    break;

    case 0xb:
    {
        uint8_t RS = instruction.byte.b1 >> 4;
        uint8_t rBase = instruction.byte.b1 & 0b1111;
        uint8_t sclPow = instruction.byte.b2 >> 4;
        int16_t offset = ((instruction.byte.b2 & 0b1111) << 8) | instruction.byte.b3;

        int64_t longaddress = registers[rBase];
        longaddress += (offset << sclPow);
        uint32_t address = longaddress;
        // printf("address %08x + (%d * (2 ^ %d)) = %08x\n", registers[rBase], offset, sclPow, address);

        switch (nBytes)
        {
        case 1:
            writeB(address, registers[RS]);
            break;
        case 2:
            writeH(address, registers[RS]);
            break;
        case 4:
            writeW(address, registers[RS]);
            break;
        default:
            printf("Illegal nBytes argument to movOp!\n");
            exit(1);
        };
    }
    break;

    case 0xf:
    {
        uint8_t RD = instruction.byte.b1 >> 4;
        switch (nBytes)
        {
        case 1:
            registers[RD] = instruction.byte.b3;
            break;
        case 2:
            registers[RD] = instruction.hword.h1;
            break;
        case 4:
        default:
            printf("Illegal nBytes argument to movOp!\n");
            exit(1);
        };
    }
    break;

    default:
        printf("Illegal movOp instruction!\n");
        exit(1);
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
    registers[sp] = 0xfffffffc;
    registers[ip] = readW(0); // read the entry point to the code segment
    printf("Read entry address of %08x\n", registers[ip]);
    bool running = true;
    uint32_t instructionCount = 0;
    while (running)
    {
        instructionData instruction = {0};
        instruction.word = consumeW(registers[ip]);

        // instruction.byte.b0 = consumeB(registers[ip]);
        // instruction.byte.b1 = consumeB(registers[ip]);
        // {
        // uint8_t swapped = instruction.byte.b0;
        // instruction.byte.b0 = instruction.byte.b1;
        // instruction.byte.b1 = swapped;
        // }

        if (opcodeNames.count(instruction.byte.b0) == 0)
        {
            printf("Unexpected opcode %2x at %08x\n", instruction.byte.b0, registers[ip] - 4);
            exit(1);
        }
        else
        {
#ifdef PRINTEXECUTION
            std::string opcodeName = opcodeNames[instruction.byte.b0];
            printf("%08x: %02x:%-4s ", registers[ip] - 4, instruction.byte.b0, opcodeName.substr(0, opcodeName.find(" ")).c_str());
#endif
        }

        // printf("%02x, %02x, %02x, %02x | %04x, %04x | %08x\n", instruction.byte.b0, instruction.byte.b1, instruction.byte.b2, instruction.byte.b3, instruction.hword.h0, instruction.hword.h1, instruction.word);

        switch (instruction.byte.b0)
        {
            // hlt/no instruction
        case 0x00:
            running = 0;
            break;

        case 0x01: // nop
            break;

        // JMP
        case 0x11:
        {
            printf("\n");
            // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", flags[NF], flags[ZF], flags[CF], flags[VF]);
            jmpOp(instruction.word & 0x00ffffff);
        }
        break;

        // JE/JZ
        case 0x13:
        {
            // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", flags[NF], flags[ZF], flags[CF], flags[VF]);

            if (flags[ZF])
            {
#ifdef PRINTEXECUTION
                printf("TAKEN\n");
#endif
                jmpOp(instruction.word & 0x00ffffff);
            }
#ifdef PRINTEXECUTION
            else
            {
                printf("NOT TAKEN\n");
            }
#endif
        }
        break;

        // JNE/JNZ
        case 0x15:
        {
            // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", flags[NF], flags[ZF], flags[CF], flags[VF]);

            if (!flags[ZF])
            {
#ifdef PRINTEXECUTION
                printf("TAKEN\n");
#endif
                jmpOp(instruction.word & 0x00ffffff);
            }
#ifdef PRINTEXECUTION
            else
            {
                printf("NOT TAKEN\n");
            }
#endif
        }
        break;

        // JG
        case 0x17:
        {
            // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", flags[NF], flags[ZF], flags[CF], flags[VF]);

            if ((!flags[ZF]) && flags[CF])
            {
#ifdef PRINTEXECUTION
                printf("TAKEN\n");
#endif
                jmpOp(instruction.word & 0x00ffffff);
            }
#ifdef PRINTEXECUTION
            else
            {
                printf("NOT TAKEN\n");
            }
#endif
        }
        break;

        // JL
        case 0x19:
        {
            // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", flags[NF], flags[ZF], flags[CF], flags[VF]);

            if (!flags[CF])
            {
#ifdef PRINTEXECUTION
                printf("TAKEN\n");
#endif
                jmpOp(instruction.word & 0x00ffffff);
            }
#ifdef PRINTEXECUTION
            else
            {
                printf("NOT TAKEN\n");
            }
#endif
        }
        break;

        // JGE
        case 0x1b:
        {
            // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", flags[NF], flags[ZF], flags[CF], flags[VF]);

            if (flags[CF])
            {
#ifdef PRINTEXECUTION
                printf("TAKEN\n");
#endif
                jmpOp(instruction.word & 0x00ffffff);
            }
#ifdef PRINTEXECUTION
            else
            {
                printf("NOT TAKEN\n");
            }
#endif
        }
        break;

        // JLE
        case 0x1d:
        {
            // printf("NF: %d ZF: %d CF: %d VF: %d\n\n", flags[NF], flags[ZF], flags[CF], flags[VF]);

            if (flags[ZF] || (!flags[CF]))
            {
#ifdef PRINTEXECUTION
                printf("TAKEN\n");
#endif
                jmpOp(instruction.word & 0x00ffffff);
            }
#ifdef PRINTEXECUTION
            else
            {
                printf("NOT TAKEN\n");
            }
#endif
        }
        break;

        // unsigned arithmetic
        case 0x41:
        case 0x43:
        case 0x45:
        case 0x47:
        case 0x49:
        case 0x4b:
        case 0x4d:
        case 0x4f:
        case 0x51:
        case 0x53: // simple arithmetic
        {
            uint8_t RD = instruction.byte.b1 >> 4;
            uint8_t RS1 = instruction.byte.b1 & 0b1111;
            uint8_t RS2 = instruction.byte.b2 >> 4;
#ifdef PRINTEXECUTION
            printf("%%r%d, %%r%d, %%r%d\n", RD, RS1, RS2);
#endif
            arithmeticOp(RD, registers[RS1], registers[RS2], instruction.byte.b0);
        }
        break;

        // INC
        case 0x54:
        {
            uint8_t RD = instruction.byte.b1 >> 4;
#ifdef PRINTEXECUTION
            printf("%%r%d\n", RD);
#endif
            registers[RD]++;
        }
        break;

        // DEC
        case 0x56:
        {
            uint8_t RD = instruction.byte.b1 >> 4;
#ifdef PRINTEXECUTION
            printf("%%r%d\n", RD);
#endif
            registers[RD]--;
        }
        break;

        // NOT
        case 0x58:
        {
            uint8_t RD = instruction.byte.b1 >> 4;
            uint8_t RS1 = instruction.byte.b1 & 0b1111;
#ifdef PRINTEXECUTION
            printf("%%r%d, %%r%d\n", RD, RS1);
#endif
            registers[RD] = ~registers[RS1];
        }
        break;

        case 0x61:
        case 0x63:
        case 0x65:
        case 0x67:
        case 0x69:
        case 0x6b:
        case 0x6d:
        case 0x6f:
        case 0x71:
        case 0x73:
            // immediate arithmetic
            {
                uint8_t RD = instruction.byte.b1 >> 4;
                uint8_t RS1 = instruction.byte.b1 & 0b1111;
                uint16_t imm = instruction.hword.h1;
#ifdef PRINTEXECUTION
                printf("%%r%d, %%r%d, %u\n", RD, RS1, imm);
#endif
                arithmeticOp(RD, registers[RS1], imm, instruction.byte.b0);
            }
            break;

        // movb
        case 0xa0:
        case 0xa2:
        case 0xa4:
        case 0xa5:
        case 0xa7:
        case 0xa9:
        case 0xab:
        case 0xaf:
        {
#ifdef PRINTEXECUTION
            printf("\n");
#endif
            movOp(instruction, 1);
        }
        break;

        // movh
        case 0xb0:
        case 0xb2:
        case 0xb4:
        case 0xb5:
        case 0xb7:
        case 0xb9:
        case 0xbb:
        case 0xbf:
        {
#ifdef PRINTEXECUTION
            printf("\n");
#endif
            movOp(instruction, 2);
        }
        break;

        // mov
        case 0xc0:
        case 0xc2:
        case 0xc4:
        case 0xc5:
        case 0xc7:
        case 0xc9:
        case 0xcb:
        {
#ifdef PRINTEXECUTION
            printf("\n");
#endif
            movOp(instruction, 4);
        }
        break;

        // stack, call/return

        // PUSH
        case 0xd0:
        {
            uint8_t RS = instruction.byte.b1 & 0b1111;
#ifdef PRINTEXECUTION
            printf("%%r%d\n", RS);
#endif
            stackPush(registers[RS]);
        }
        break;

        // PUSHI
        case 0xd1:
        {
            uint32_t imm = instruction.word & 0x00ffffff;
#ifdef PRINTEXECUTION
            printf("%u\n", imm);
#endif
            stackPush(imm);
        }
        break;

        // POP
        case 0xd2:
        {
            uint8_t RD = instruction.byte.b1 & 0b1111;
#ifdef PRINTEXECUTION
            printf("%u\n", RD);
#endif
            // fault condition by which attempt to pop from a completely empty stack
            if (registers[sp] >= 0xfffffffc)
            {
                printf("Stack underflow fault\n");
                exit(1);
            }

            registers[RD] = stackPop();
        }
        break;

        // CALL
        case 0xd3:
        {
            uint32_t callAddr = instruction.word << 8;
#ifdef PRINTEXECUTION
            printf("%08x\n", callAddr);
#endif
            // printf("call to %08x\n", callAddr);
            stackPush(registers[bp]);
            stackPush(registers[ip]);
            registers[bp] = registers[sp];
            registers[ip] = callAddr;
        }
        break;

        // RET
        case 0xd4:
        {
            uint32_t argw = instruction.word & 0x00ffffff;
#ifdef PRINTEXECUTION
            printf("%d\n", argw);
#endif
            registers[ip] = stackPop();
            registers[bp] = stackPop();
            registers[sp] += argw;
        }
        break;

            /*
            ase 0xd0: // CALL
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
                break;*/

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

            /*case 0xa8:
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
                uint16_t sourceAddress = registers[RS];
                uint8_t RD = operands >> 6 & 0b11111;
                registers[RD] = readByte(sourceAddress) << 8;
                registers[RD] |= readByte(sourceAddress + 1);
            }
            break;

            case 0xaa: // register -> dereferenced register mov
            {
                uint16_t operands = consumeWord(registers[ip]);
                uint8_t RS = operands >> 11;
                uint16_t sourceAddress = registers[RS];
                uint8_t RD = operands >> 6 & 0b11111;
                writeByte(registers[RD], sourceAddress >> 8);
                writeByte(registers[RD] + 1, sourceAddress);
            }
            break;

            // (register + offset) dereferenced -> register
            case 0xab: // MOV
            {
                uint16_t operands = consumeWord(registers[ip]);
                uint8_t RS = operands >> 11;
                uint16_t sourceAddress = registers[RS];
                uint8_t RD = operands >> 6 & 0b11111;
                int16_t offset = consumeWord(registers[ip]);
                registers[RD] = readByte(sourceAddress + offset) << 8;
                registers[RD] |= readByte(sourceAddress + offset + 1);
            }
            break;

            case 0xac: // MOV offset(rd), rs
            {
                uint16_t operands = consumeWord(registers[ip]);
                uint8_t RS = operands >> 11;
                uint16_t sourceAddress = registers[RS];
                uint8_t RD = (operands >> 6) & 0b11111;
                int16_t offset = consumeWord(registers[ip]);
                writeByte(registers[RD] + offset, sourceAddress >> 8);
                writeByte(registers[RD] + offset + 1, sourceAddress & 0b11111111);
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

            case 0xae: // MOV roffset(rd, scale), rS
            {
                uint16_t operands = consumeWord(registers[ip]);
                uint8_t RS = operands >> 11;
                uint16_t sourceAddress = registers[RS];
                uint8_t RD = operands >> 6 & 0b11111;
                uint8_t RO = operands >> 1 & 0b11111;
                uint8_t scale = consumeByte(registers[ip]);

                uint16_t address = registers[RD] + (scale * registers[RO]);
                writeByte(address, sourceAddress >> 8);
                writeByte(address + 1, sourceAddress & 0b11111111);
            }
            break;

            case 0xaf: // MOV reg, imm
            {
                uint8_t insByte2 = consumeByte(registers[ip]);
                uint8_t RD = insByte2 & 0b11111;
                uint16_t imm = consumeWord(registers[ip]);
                registers[RD] = imm;
            }
            break;

            case 0xb0:
            {
                uint16_t operands = consumeByte(registers[ip]);
                uint16_t offset = consumeWord(registers[ip]);
                uint16_t immediate = consumeWord(registers[ip]);
                uint8_t RD = operands & 0b11111;

                uint16_t address = registers[RD] + offset;
                writeByte(address, immediate >> 8);
                writeByte(address + 1, immediate & 0b11111111);
            }
            break;

            case 0xb1:
            {
                uint16_t operands = consumeWord(registers[ip]);
                uint16_t immediate = consumeWord(registers[ip]);
                uint8_t RD = operands >> 6 & 0b11111;
                uint8_t RO = operands >> 1 & 0b11111;
                uint8_t scale = consumeByte(registers[ip]);

                uint16_t address = registers[RD] + (scale * registers[RO]);
                writeByte(address, immediate >> 8);
                writeByte(address + 1, immediate & 0b11111111);
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
            */

        case 0xe2:
        {
            uint8_t rs = instruction.byte.b1 & 0xf;
            printf("\t\t%%r%d: %u\n", rs, registers[rs]);
        }
        break;

        case 0xfe:
            running = false;
            break;

        default:
            printf("\n\n\nError: Undefined opcode %02x at memory loctaion %02x\n", instruction.byte.b0, registers[ip - 1]);
            printf("Instruction was: %02x, %02x, %02x, %02x | %04x, %04x | %08x\n", instruction.byte.b0, instruction.byte.b1, instruction.byte.b2, instruction.byte.b3, instruction.hword.h0, instruction.hword.h1, instruction.word);
            exit(1);
        }

        // printState();
        // printf("\n");
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(25ms);
        }

        // printf("\n");

        instructionCount++;
    }
    printState();
    std::cout << "Execution halted after " << instructionCount << " instructions" << std::endl;
    std::cout << "opening dump file" << std::endl;
    std::ofstream dumpFile;
    dumpFile.open("memdump.bin", std::ofstream::out);
    std::cout << "dump file opened" << std::endl;
    for (uint32_t pageIndex : memory.ActivePages())
    {
        char pageHeader[17];
        sprintf(pageHeader, "Page add%08x", pageIndex << PAGE_BIT_WIDTH);
        dumpFile << pageHeader;
        for (int i = 0; i < PAGE_SIZE; i++)
        {
            dumpFile.put(memory.ReadByte((pageIndex << PAGE_BIT_WIDTH) + i));
        }
    }
    dumpFile.close();
}