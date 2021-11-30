# The Architecture
 - 16-bit address space and registers
 - 16 registers (8086 style)
   - a/b/c/d (general purpose but same conventions as x86)
   - r0-r7 (completely general purpose for scratch use)
   - Source
   - Destination
   - Stack Pointer
   - Base Pointer
   - Instruction Pointer is segregated, untouchable
 - Flags:
   - Zero, Carry, Sign, Overflow
# The ISA
 - 8 bit instruction opcodes
 - Variable instruction length
## Instructions separated into 4 basic types
### Control Flow
#### opcodes 0x00-0x3f
 - 0x00: NOP
### I/O
#### opcodes 0x40-0x7f
### Arithmetic
 - #### Simple arithmetic: \[OP] %\[RS] %\[RD]
 - ##### RD = RD OP RS
   - 0x40: ADD - add
   - 0x41: SUB - subtract
   - 0x42: MUL - multiply
   - 0x43: DIV - divide
   - 0x44: SHR - shift right
   - 0x45: SHL - shift left
   - 0x46: INC - increment
   - 0x47: DEC - decrement
 - #### Immediate arithmetic: **(** \[OP] **)(** 0b0000 %\[RD] **)** (([IMM16]))
   - 0x48: ADDI add immediate
   - 0x49: SUBI 
   - 0x4a: MULI
   - 0x4b: DIVI
   - 0x4c: SHRI
   - 0x4d: SHLI
   - 0x4e: UNUSED
   - 0x4f: UNUSED
#### opcodes 0x80-0xbf
### Data Transfer
 - #### MOVs
 - 0x80: MOV %[RS] %[RD] register -> register
 - 0x81: MOV %[RS] (%[RD]) register -> dereferenced register
 - 0x82: MOV (%[RS]) %[RD] dereferenced register -> register
 -  #### Stack
 - 0x88: PUSH %[RS] -> push RS to stack
 - 0x89: POP %[RD] -> pop stack to RD
#### opcodes 0xc0-0xff