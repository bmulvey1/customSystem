fibb:
	push %r1
	push %r2
	sub %sp, $14
	mov %r0, $1
	mov %r1, 4(%bp)
	cmp %r1, $1
	jg fibb_1
	mov %r1, $1
	mov %r2, %r1
	add %r2, $1
	add %r2, %r1
	mov %r0, 4(%bp)
	mov -2(%bp), %r1
	mov -6(%bp), %r2
	jmp fibb_2
fibb_1:
	mov %r1, 4(%bp)
	sub %r1, $2
	push %r1
	mov %r1, %r0
	call fibb
	mov %r2, 4(%bp)
	sub %r2, $1
	push %r2
	mov %r2, %r0
	call fibb
	mov %r1, %r0
	add %r1, %r2
	mov %r0, %r1
fibb_2:
	jmp fibb_done
fibb_done:
	add %sp, $14
	pop %r2
	pop %r1
	ret 1
