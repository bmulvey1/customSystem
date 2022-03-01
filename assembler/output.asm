	#include "CPU.asm"
	entry code
code:
	push $20
	call firstnfibs
	hlt
Program_done:
	ret 0
fib:
	sub %sp, $2
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
	add %sp, $2
	ret 2
firstnfibs:
	sub %sp, $2
	push %r1
	push %r2
	mov %r0, $0
firstnfibs_1:
	cmp %r0, 4(%bp)
	jge firstnfibs_2
	mov %r1, %r0
	add %r1, $1
	push %r1
	mov %r2, %r0
	call fib
	out %r0
	add %r2, $1
	mov %r0, -2(%bp)
	mov %r0, %r2
	jmp firstnfibs_1
firstnfibs_2:
firstnfibs_done:
	pop %r2
	pop %r1
	add %sp, $2
	ret 2
