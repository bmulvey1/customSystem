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
firstnfibs:
	sub %sp, $4
	push %r1
	mov %r0, $0
firstnfibs_1:
	cmp %r0, 4(%bp)
	jg firstnfibs_2
	push 4(%bp)
	mov %r1, %r0
	call fib
	mov %r1, %r1
	add %r1, $1
	mov %r0, -2(%bp)
	mov %r0, %r1
	jmp firstnfibs_1
firstnfibs_2:
firstnfibs_done:
	pop %r1
	add %sp, $4
	ret 1
