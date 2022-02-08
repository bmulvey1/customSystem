fibb:
	sub %sp, $2
	push %r1
	mov %r0, 4(%bp)
	cmp %r0, $1
	jg fibb_1
	mov %r0, 4(%bp)
	mov -2(%bp), %r0
	jmp fibb_2
fibb_1:
	mov %r0, 4(%bp)
	sub %r0, $2
	push %r0
	call fibb
	mov %r1, 4(%bp)
	sub %r1, $1
	push %r1
	mov %r1, %r0
	call fibb
	add %r1, %r0
	mov -2(%bp), %r1
fibb_2:
	mov %r0, -2(%bp)
	jmp fibb_done
fibb_done:
	pop %r1
	add %sp, $2
	ret 1
