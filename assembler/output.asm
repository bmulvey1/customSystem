	#include "CPU.asm"
	entry code
code:
	push $20
	call firstnfibs
	hlt
Program_done:
	ret 0
firstNfibs:
	sub %sp, $2
	push %r1
	push %r2
	push %r3
	push %r4
	mov %r0, $1000
	mov %r1, (%r0)
	mov %r2, %r1
	sub %r2, $2
	mov %r3, %r2
	mul %r3, $2
	add %r3, %r0
	mov %r2, (%r3)
	mov %r3, %r1
	sub %r3, $1
	mov %r4, %r3
	mul %r4, $2
	add %r4, %r0
	mov %r3, (%r4)
	add %r2, %r3
	mov %r3, %r1
	mul %r3, $2
	add %r3, %r0
	mov (%r3), %r2
firstNfibs_done:
	pop %r4
	pop %r3
	pop %r2
	pop %r1
	add %sp, $2
	ret 2
