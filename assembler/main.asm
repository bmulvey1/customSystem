	#include "CPU.asm"
	entry code
code:
	push $20
	call fib
	;call firstkprimes
code_done:
	hlt
	ret 0
print:
	push %r1
	push %r0
print_0:
	;introduce var i to %r0
	mov %r0, 4(%bp) ;place argument i
	;introduce var j to %r1
	mov %r1, %r0;j = i
	out %r1
	pop %r0
	pop %r1
	ret 2
fib:
	sub %sp, $2
	push %r5
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
fib_0:
	;introduce var n to %r0
	mov %r0, 4(%bp) ;place argument n
	;introduce var fibarr to %r1
	mov %r1, $512;fibarr = 512
		;introduce var .t0 to %r2
	mov %r2, $0;.t0 = 0
	mov (%r1), %r2;(fibarr) = .t0
		;introduce var .t1 to %r2
	mov %r2, $1;.t1 = 1
		;introduce var .t2 to %r3
	mov %r3, %r1
	add %r3, $2;.t2 = fibarr + 2
	mov (%r3), %r2;(.t2) = .t1
		;introduce var j to %r2
		;introduce var i to %r3
	mov %r3, $2;i = 2
fib_2:
	cmp %r3, %r0;cmp i n
	jg fib_1
		;introduce var .t7 to %r4
	mov %r4, %r3
	sub %r4, $1;.t7 = i - 1
		;introduce var .t6 to %r5
	mov %r5, %r4
	mul %r5, $2;.t6 = .t7 * 2
		;introduce var .t5 to %r4
	mov %r4, %r1
	add %r4, %r5;.t5 = fibarr + .t6
		;introduce var .t4 to %r5
	mov %r5, (%r4);.t4 = (.t5)
		;introduce var .t11 to %r4
	mov %r4, %r3
	sub %r4, $2;.t11 = i - 2
	mov -2(%bp), %r5;spill          .t4
		;introduce var .t10 to %r5
	mov %r5, %r4
	mul %r5, $2;.t10 = .t11 * 2
		;introduce var .t9 to %r4
	mov %r4, %r1
	add %r4, %r5;.t9 = fibarr + .t10
		;introduce var .t8 to %r5
	mov %r5, (%r4);.t8 = (.t9)
	mov %r2, -2(%bp)
	add %r2, %r5;j = .t4 + .t8
		;introduce var .t12 to %r5
	mov %r5, %r2;.t12 = j
		;introduce var .t14 to %r4
	mov %r4, %r3
	mul %r4, $2;.t14 = i * 2
	mov -2(%bp), %r5;spill         .t12
		;introduce var .t13 to %r5
	mov %r5, %r1
	add %r5, %r4;.t13 = fibarr + .t14
	mov %r4, -2(%bp);unspill     .t12
	mov (%r5), %r4;(.t13) = .t12
	mov %r3, %r3
	add %r3, $1;i = i + 1
	jmp fib_2
fib_1:
	mov %r3, $1;i = 1
fib_4:
	cmp %r3, %r0;cmp i n
	jg fib_3
		;introduce var .t19 to %r2
	mov %r2, %r3
	mul %r2, $2;.t19 = i * 2
		;introduce var .t18 to %r4
	mov %r4, %r1
	add %r4, %r2;.t18 = fibarr + .t19
		;introduce var .t17 to %r2
	mov %r2, (%r4);.t17 = (.t18)
	push %r2
	call print
	mov %r3, %r3
	add %r3, $1;i = i + 1
	jmp fib_4
fib_3:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	add %sp, $2
	ret 2
