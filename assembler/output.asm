	#include "CPU.asm"
	entry code
code:
	push $20
	call firstnfibs
	hlt
Program_done:
	ret 0
firstnfibs:
	sub %sp, $2
	push %r1
	push %r2
	push %r3
	push %r4
	push %r5
	push %r6
	push %r7
	push %r8
	push %r9
	mov %r0, $1000
	mov %r1, $0
	mov (%r0), %r1
	mov %r1, $1
	mov %r2, %r0
	add %r2, $2
	mov (%r2), %r1
	mov %r1, $2
firstnfibs_1:
	cmp %r1, 4(%bp)
	jg firstnfibs_2
	mov %r2, %r1
	sub %r2, $2
	mov %r3, %r2
	mul %r3, $2
	add %r3, %r0
	mov %r4, (%r3)
	mov %r5, %r1
	sub %r5, $1
	mov %r6, %r5
	mul %r6, $2
	add %r6, %r0
	mov %r7, (%r6)
	add %r4, %r7
	mov %r8, %r4
	mov %r9, %r1
	mul %r9, $2
	add %r9, %r0
	mov (%r9), %r8
	out %r4
	add %r1, $1
	mov -6(%bp), %r4
	jmp firstnfibs_1
firstnfibs_2:
firstnfibs_done:
	pop %r9
	pop %r8
	pop %r7
	pop %r6
	pop %r5
	pop %r4
	pop %r3
	pop %r2
	pop %r1
	add %sp, $2
	ret 2
