#include "CPU.asm"
entry code

code:
	push $14
	call fibrev
code_done:
	hlt

fib:
	sub %sp, $0
	push %r1
	mov %r0, 4(%bp)
	cmp %r0, $2
	jge fib_1
	mov %r0, 4(%bp)
	jmp fib_done
fib_1:
	mov %r0, 4(%bp)
	sub %r0, $2
	push %r0
	call fib
	mov %r1, 4(%bp)
	sub %r1, $1
	push %r1
	mov %r1, %r0
	call fib
	add %r1, %r0
	mov %r0, %r1
	jmp fib_done
fib_done:
	pop %r1
	add %sp, $0
	ret 1
fibrev:
	sub %sp, $4
	push %r1
	push %r2
	mov %r0, $20
	mov %r1, 4(%bp)
	mov %r2, %r0
	sub %r2, %r1
	push %r2
	call fib
	out %r0
fibrev_done:
	pop %r2
	pop %r1
	add %sp, $4
	ret 1
