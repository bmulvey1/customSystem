#include "CPU.asm"
entry code
code:
	add %r0, $1
	cmp %r0, $20
	jge code_done
	push %r0
	call fib
	out %rr
	jmp code
code_done:
	hlt
	


fib:
	push %r3
	push %r2
	push %r1
	push %r0
fib_0:
		;introduce var i to %r0
	mov %r0, 4(%bp) ;place argument i
		;introduce var result to %r1
	cmp %r0, $2;cmp i 2
	jge fib_2
	mov %r1, %r0;result = i
	jmp fib_1
fib_2:
		;introduce var .t2 to %r2
	mov %r2, %r0
	sub %r2, $2;.t2 = i - 2
	push %r2
		;introduce var .t1 to %r2
	call fib
	mov %r2, %rr;.t1 = call fib
		;introduce var .t4 to %r3
	mov %r3, %r0
	sub %r3, $1;.t4 = i - 1
	push %r3
		;introduce var .t3 to %r3
	call fib
	mov %r3, %rr;.t3 = call fib
	mov %r1, %r2
	add %r1, %r3;result = .t1 + .t3
	jmp fib_1
fib_1:
		;introduce var .RETVAL to %r0
	mov %r0, %r1;.RETVAL = result
	mov %rr, %r0;ret .RETVAL
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	ret 2
