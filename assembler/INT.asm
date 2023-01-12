; #include "CPU.asm"


INT_UND_OP:
    mov %rr, (%bp+8)
    movh %r0, $0
    movh %r1, $ INT_UND_OP_STR
    INT_UND_OP_STR_LOOP:
        mov %r3, (%r1+%r0,1)
        cmpi %r3, $0
        je INT_UND_OP_STR_LOOP_DONE
        out 0, %r3
        inc %r0
        jmp INT_UND_OP_STR_LOOP
    INT_UND_OP_STR_LOOP_DONE:
        hlt;
    INT_UND_OP_STR:
        #d "Fatal Exception: Undefined Opcode\0"

INT_UND_INT:
    mov %rr, (%bp+8)
    movh %r0, $0
    movh %r1, $ INT_UND_OP_STR
    INT_UND_INT_STR_LOOP:
        mov %r3, (%r1+%r0,1)
        cmpi %r3, $0
        je INT_UND_OP_STR_LOOP_DONE
        out 0, %r3
        inc %r0
        jmp INT_UND_OP_STR_LOOP
    INT_UND_INT_STR_LOOP_DONE:
        hlt;
    INT_UND_INT_STR:
        #d "Fatal Exception: Undefined Interrupt\0"



INT_PC_NOT_ALIGNED:
    mov %rr, (%bp+8)
    movh %r0, $0
    movh %r1, $ PC_NOT_ALIGNED
    PC_NOT_ALIGNED_STR_LOOP:
        mov %r3, (%r1+%r0,1)
        cmpi %r3, $0
        je PC_NOT_ALIGNED_STR_LOOP_DONE
        out 0, %r3
        inc %r0
        jmp PC_NOT_ALIGNED_STR_LOOP
    PC_NOT_ALIGNED_STR_LOOP_DONE:
        hlt;
    PC_NOT_ALIGNED_STR:
        #d "Fatal Exception: Program Counter Not Aligned\0"

INT_OUT_PORT_INVALID:
    mov %rr, (%bp+8)
    movh %r0, $0
    movh %r1, $ PC_NOT_ALIGNED
    OUT_PORT_INVALID_LOOP:
        mov %r3, (%r1+%r0,1)
        cmpi %r3, $0
        je OUT_PORT_INVALID_DONE
        out 0, %r3
        inc %r0
        jmp PC_NOT_ALIGNED_STR_LOOP
    OUT_PORT_INVALID_LOOP_DONE:
        hlt;
    OUT_PORT_INVALID_STR:
        #d "Fatal Exception: Output Port Not Configured\0"

INT_PRINT_CHAR:
    out 0, %rr
    reti









#addr 64512