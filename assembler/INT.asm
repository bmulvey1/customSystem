#include "CPU.asm"

IDT:
int 0x01      ; INT0 - execute INT1
#d32 $UND_INT ; INT1 - Undefined Interrupt
#d32 $UND_OP ; INT2 - Undefined Opcode
#d32 $PC_NOT_ALIGNED ; INT3 - Program Counter not Aligned
#d32 $OUT_PORT_INVALID ; INT4 - Selected Port is Invalid


#align 8192 ; first 256 * 4 bytes are the addresses of the handlers

INT_UND_OP:
    mov %rr, (%bp+8)
    movh %r0, 0
    movh %r1, % INT_UND_OP_STR
INT_UND_OP_STR_LOOP:
    mov %r3, (%r1+%r0,1)
    cmpi %r3, 0
    je INT_UND_OP_STR_LOOP_DONE
    out 0, %r3
    addi %r0, 1
    jmp INT_UND_OP_STR_LOOP
INT_UND_OP_STR_LOOP_DONE:
    hlt;
INT_UND_OP_STR:
    #d "Fatal Exception: Undefined Opcode\0"


PC_NOT_ALIGNED:
    mov %rr, (%bp+8)
    movh %r0, 0
    movh %r1, % PC_NOT_ALIGNED
PC_NOT_ALIGNED_STR_LOOP:
    mov %r3, (%r1+%r0,1)
    cmpi %r3, 0
    je PC_NOT_ALIGNED_STR_LOOP_DONE
    out 0, %r3
    addi %r0, 1
    jmp PC_NOT_ALIGNED_STR_LOOP
PC_NOT_ALIGNED_STR_LOOP_DONE:
    hlt;
PC_NOT_ALIGNED_STR:
    #d "Fatal Exception: Program Counter Not Aligned\0"

OUT_PORT_INVALID:
    mov %rr, (%bp+8)
    movh %r0, 0
    movh %r1, % PC_NOT_ALIGNED
OUT_PORT_INVALID_LOOP:
    mov %r3, (%r1+%r0,1)
    cmpi %r3, 0
    je OUT_PORT_INVALID_DONE
    out %r3
    addi %r0, 1
    jmp PC_NOT_ALIGNED_STR_LOOP
OUT_PORT_INVALID_LOOP_DONE:
    hlt;
OUT_PORT_INVALID_STR:
    #d "Fatal Exception: Output Port Not Configured\0"



#addr 65536