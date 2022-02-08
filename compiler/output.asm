fibb:
	sub %sp, $14
	push %r1
	push %r2
	push %r3
	mov %r0, $1
	mov %r1, 4(%bp)
	cmp %r1, $1
	jg fibb_1
	mov %r1, $10
	sub %r1, 4(%bp)
	mov %r0, 4(%bp)
	mov %r2, %r1
	add %r2, $1
	mov %r3, %r1
	add %r3, %r2
	mov -2(%bp), %r1
	mov -4(%bp), %r2
	mov -6(%bp), %r3
	jmp fibb_2
fibb_1:
	mov %r1, $1
	mov %r2, 4(%bp)
	sub %r2, $2
	push %r2
	mov %r2, %r0
	call fibb
	mov %r3, 4(%bp)
	sub %r3, $1
	push %r3
	mov %r3, %r0
	call fibb
	mov %r2, %r0
	add %r2, %r3
	mov %r0, $1
	add %r0, 4(%bp)
	add %r0, %r1
	add %r0, %r1
	mov %r0, -14(%bp)
	mov %r0, %r2
	mov -2(%bp), %r1
fibb_2:
	mov %r1, $1
	sub %r1, -2(%bp)
	mov %r1, $1
	sub %r1, -4(%bp)
	mov %r1, $1
	sub %r1, -6(%bp)
	jmp fibb_done
fibb_done:
	pop %r3
	pop %r2
	pop %r1
	add %sp, $14
	ret 1
