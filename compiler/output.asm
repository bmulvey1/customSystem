fibb:
	sub %sp, $0
	push %r1
	mov %r0, 4(%bp)
	cmp %r0, $2
	jge fibb_1
	mov %r0, 4(%bp)
	jmp fibb_done
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
	mov %r0, %r1
	jmp fibb_done
fibb_done:
	pop %r1
	add %sp, $0
	ret 1
isfirstthree:
	sub %sp, $2
	mov %r0, 4(%bp)
	cmp %r0, $1
	jne isfirstthree_1
	mov %r0, $1
	mov -2(%bp), %r0
	jmp isfirstthree_2
isfirstthree_1:
	mov %r0, 4(%bp)
	cmp %r0, $2
	jne isfirstthree_3
	mov %r0, $2
	mov -2(%bp), %r0
	jmp isfirstthree_4
isfirstthree_3:
	mov %r0, 4(%bp)
	cmp %r0, $3
	jne isfirstthree_5
	mov %r0, $3
	mov -2(%bp), %r0
	jmp isfirstthree_6
isfirstthree_5:
	mov %r0, $0
	mov -2(%bp), %r0
isfirstthree_6:
isfirstthree_4:
isfirstthree_2:
	mov %r0, -2(%bp)
	jmp isfirstthree_done
isfirstthree_done:
	add %sp, $2
	ret 1
