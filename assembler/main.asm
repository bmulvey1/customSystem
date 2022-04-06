	#include "CPU.asm"
	entry code
code:
	add %r0, $1
	cmp %r0, $20
	jg code_done
	push %r0
	call fib
	out %rr
	jmp code;
code_done:
	hlt
	ret 0
fib:
	sub %sp, $6
	push %r3
	push %r2
	push %r1
	push %r0
fib_0:
	;introduce var i to %r0
	mov %r0, 4(%bp) ;place argument i
	;introduce var result to %r1
	;introduce var x to %r2
	mov %r2, $1;x = 1
		;introduce var y to %r3
	mov %r3, $2;y = 2
	mov -2(%bp), %r0;spill            i
		;introduce var z to %r0
	mov %r0, $3;z = 3
	mov -4(%bp), %r2;spill            x
	mov %r2, -2(%bp);unspill        i
	cmp %r2, $2;cmp i 2
	jge fib_4
	cmp %r2, $1;cmp i 1
	jge fib_3
	mov %r1, $0;result = 0
	jmp fib_2
fib_3:
	mov %r1, $1;result = 1
	jmp fib_2
fib_2:
	jmp fib_1
fib_4:
	mov -2(%bp), %r3;spill            y
		;introduce var .t2 to %r3
	mov %r3, %r2
	sub %r3, $2;.t2 = i - 2
	push %r3
		;introduce var .t1 to %r3
	call fib
	mov %r3, %rr;.t1 = call fib
	mov -6(%bp), %r3;spill          .t1
		;introduce var .t4 to %r3
	mov %r3, %r2
	sub %r3, $1;.t4 = i - 1
	push %r3
		;introduce var .t3 to %r3
	call fib
	mov %r3, %rr;.t3 = call fib
	mov %r1, -6(%bp)
	add %r1, %r3;result = .t1 + .t3
	mov %r3, -2(%bp)
	jmp fib_1
fib_1:
	mov %r2, -4(%bp);unspill        x
	mov %r2, %r2;x = x
	mov %r3, %r3;y = y
	mov %r0, %r0;z = z
		;introduce var .RETVAL to %r0
	mov %r0, %r1;.RETVAL = result
	mov %rr, %r0;ret .RETVAL
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	add %sp, $6
	ret 2
