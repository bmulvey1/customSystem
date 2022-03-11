	#include "CPU.asm"
	entry code
code:
	push $20
	call firstkprimes
	hlt
Program_done:
	ret 0
firstkprimes:
	push %r1
	push %r2
	push %r3
	push %r4
	mov %r0, $2000
	mov %r1, $2
firstkprimes_1:
	cmp %r1, $1000
	jg firstkprimes_2
	mov %r2, %r1
	mul %r2, $2
	add %r2, %r0
	mov %r3, (%r2)
	cmp %r3, $0
	jne firstkprimes_3
	mov %r2, %r1
firstkprimes_4:
	cmp %r2, $1000
	jg firstkprimes_5
	mov %r3, $1
	mov %r4, %r2
	mul %r4, $2
	add %r4, %r0
	mov (%r4), %r3
	mov %r2, %r2
	add %r2, %r1
	jmp firstkprimes_4
firstkprimes_5:
	out %r1
	mov -6(%bp), %r2
firstkprimes_3:
	add %r1, $1
	jmp firstkprimes_1
firstkprimes_2:
firstkprimes_done:
	pop %r4
	pop %r3
	pop %r2
	pop %r1
	ret 0
