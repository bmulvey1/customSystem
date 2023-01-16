; #include "CPU.asm"

IDT:
#d32 UND_INT           ; INT 0 - Undefined Interrupt
#d32 UND_INT           ; INT 1 - Undefined Interrupt
#d32 UND_OP            ; INT 2 - Undefined Opcode
#d32 PC_NOT_ALIGNED    ; INT 3 - Program Counter not Aligned
#d32 OUT_PORT_INVALID  ; INT 4 - Selected Port is Invalid
#d32 PRINT_CHAR        ; INT 5 print a character

#align 8192 ; first 256 * 4 bytes are the addresses of the handlers


UND_OP:
	mov %rr, (%bp+8)
	movh %r0, $0
	movh %r1, $ INT_UND_OP_STR
	INT_UND_OP_STR_LOOP:
		movb %r3, (%r1+%r0,0)
		cmpi %r3, $0
		je INT_UND_OP_STR_LOOP_DONE
		out 0, %r3
		inc %r0
		jmp INT_UND_OP_STR_LOOP
	INT_UND_OP_STR_LOOP_DONE:
		mov %r0, (%sp)
		hlt;
	INT_UND_OP_STR:
		#d "Fatal Exception: Undefined Opcode\n\0"

UND_INT:
	mov %rr, (%bp+8)
	movh %r0, $0
	movh %r1, $ INT_UND_OP_STR
	INT_UND_INT_STR_LOOP:
		movb %r3, (%r1+%r0,0)
		cmpi %r3, $0
		je INT_UND_OP_STR_LOOP_DONE
		out 0, %r3
		inc %r0
		jmp INT_UND_OP_STR_LOOP
	INT_UND_INT_STR_LOOP_DONE:
		hlt;
	INT_UND_INT_STR:
		#d "Fatal Exception: Undefined Interrupt\n\0"



PC_NOT_ALIGNED:
	mov %rr, (%bp+8)
	movh %r0, $0
	movh %r1, $ PC_NOT_ALIGNED
	PC_NOT_ALIGNED_STR_LOOP:
		movb %r3, (%r1+%r0,0)
		cmpi %r3, $0
		je PC_NOT_ALIGNED_STR_LOOP_DONE
		out 0, %r3
		inc %r0
		jmp PC_NOT_ALIGNED_STR_LOOP
	PC_NOT_ALIGNED_STR_LOOP_DONE:
		mov %r0, (%sp)
		hlt;
	PC_NOT_ALIGNED_STR:
		#d "Fatal Exception: Program Counter Not Aligned\n\0"

OUT_PORT_INVALID:
	mov %rr, (%bp+8)
	movh %r0, $0
	movh %r1, $ PC_NOT_ALIGNED
	OUT_PORT_INVALID_LOOP:
		movb %r3, (%r1+%r0,0)
		cmpi %r3, $0
		je OUT_PORT_INVALID_LOOP_DONE
		out 0, %r3
		inc %r0
		jmp PC_NOT_ALIGNED_STR_LOOP
	OUT_PORT_INVALID_LOOP_DONE:
		hlt;
	OUT_PORT_INVALID_STR:
		#d "Fatal Exception: Output Port Not Configured\n\0"

PRINT_CHAR:
	out 0, %rr
	reti









#addr 65536