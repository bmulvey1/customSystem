#include "CPU.asm"
entry code
code:
    inc %r0
    cmp %r0, $20
    jg code_done
    push %r0
    call fib
    out %rr
    jmp code
code_done:
    hlt

fib:
	sub %sp, $8
	push %r2
	push %r1
	push %r0
fib_0:
		;introduce var i to %r0
	mov %r0, 4(%bp) ;place argument i
		;introduce var result to %r1
		;introduce var x to %r2
	mov %r2, $1;x = 1
	mov -2(%bp), %r0;spill            i
		;introduce var y to %r0
	mov %r0, $2;y = 2
	mov -4(%bp), %r2;spill            x
		;introduce var z to %r2
	mov %r2, $3;z = 3
	mov -6(%bp), %r0;spill            y
	mov %r0, -2(%bp)
	cmp %r0, $2;cmp i 2
	jge fib_4
	cmp %r0, $1;cmp i 1
	jge fib_3
	mov %r1, $0;result = 0
	jmp fib_2
fib_3:
	mov %r1, $1;result = 1
	jmp fib_2
fib_2:
	jmp fib_1
fib_4:
	mov -2(%bp), %r2;spill            z
		;introduce var .t2 to %r2
	mov %r2, %r0
	sub %r2, $2;.t2 = i - 2
	push %r2
		;introduce var .t1 to %r2
	call fib
	mov %r2, %rr;.t1 = call fib
	mov -8(%bp), %r2;spill          .t1
		;introduce var .t4 to %r2
	mov %r2, %r0
	sub %r2, $1;.t4 = i - 1
	push %r2
		;introduce var .t3 to %r2
	call fib
	mov %r2, %rr;.t3 = call fib
	mov %r1, -8(%bp)
	add %r1, %r2;result = .t1 + .t3
	mov %r2, -2(%bp)
	jmp fib_1
fib_1:
	mov %r0, -4(%bp)
	;mov %r1, %r0;x = x
	mov %r0, -6(%bp)
	;mov %r2, %r0;y = y
	;mov %r2, %r2;z = z
		;introduce var .RETVAL to %r2
	mov %r2, %r1;.RETVAL = result
	mov %rr, %r2;ret .RETVAL
	pop %r0
	pop %r1
	pop %r2
	add %sp, $8
	ret 2
