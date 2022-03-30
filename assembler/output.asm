fib:
	sub %sp, $2
	mov %r0, 4(%bp) ;place argument i
fib_0:
	cmp %r0, 2;cmp i 2
	jge .fib_1
	mov %r1, %r0;result = i
	jmp .fib_2
fib_1:
	mov %r2, %r0
	sub %r2, $2;.t4 = i - 2
	mov -2(%bp), %r1;spill       result
	mov %r1, %r0
	sub %r1, $1;.t2 = i - 1
	mov %r0, -2(%bp)
	mov %r0, %r1
	add %r0, %r2;result = .t1 + .t3
fib_2:
	add %sp, $2
	ret 2
