fibb:
	push %r1
	push %r2
	sub %sp, $4
	mov %r0, $1		;result = 1

	mov %r1, 4(%bp)
	cmp %r1, $1		;cmp n 1
	jg fibb_1		;jg label 1
	mov %r1, $1		;a = 1
	mov %r2, 4(%bp)		;result = n
	mov %r0, %r2
	mov 0(%bp), %r1
	jmp fibb_2		;jmp label 2
fibb_1:

	mov %r1, 4(%bp)
	sub %r1, $2		;.t4 = n - 2
	push %r1		;push .t4
	mov %r1, %r0
	call fibb		;.t3 = call fibb
	mov %r2, 4(%bp)
	sub %r2, $1		;.t2 = n - 1
	push %r2		;push .t2
	mov %r2, %r0
	call fibb		;.t1 = call fibb
	mov %r1, %r0
	add %r1, %r2		;result = .t1 + .t3 - Reorderable
	mov %r0, %r1
fibb_2:

	mov %r1, 0(%bp)		;a = a
		;.RETVAL = result
	jmp fibb_done		;ret .RETVAL
fibb_done:
	add %sp, $4
	pop %r2
	pop %r1
	ret 1
