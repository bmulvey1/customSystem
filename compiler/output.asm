lessfive:
	sub %sp, $2
	mov %r0, $7
	mov %r1, 4(%bp)
	cmp %r1, $10
	jge lessfive_1
	mov %r1, 4(%bp)
	cmp %r1, $9
	jge lessfive_2
	mov %r1, 4(%bp)
	cmp %r1, $8
	jge lessfive_3
	mov %r1, 4(%bp)
	cmp %r1, $7
	jge lessfive_4
	mov %r1, 4(%bp)
	cmp %r1, $6
	jge lessfive_5
	mov %r1, 4(%bp)
	cmp %r1, $5
	jge lessfive_6
	mov %r0, $4
	jmp lessfive_7
lessfive_6:
	mov %r0, $5
lessfive_7:
	jmp lessfive_8
lessfive_5:
	mov %r0, $6
lessfive_8:
	jmp lessfive_9
lessfive_4:
	mov %r0, $7
lessfive_9:
	jmp lessfive_10
lessfive_3:
	mov %r0, $8
lessfive_10:
	jmp lessfive_11
lessfive_2:
	mov %r0, $9
lessfive_11:
	jmp lessfive_12
lessfive_1:
	mov %r0, $10
lessfive_12:
	jmp lessfive_done
lessfive_done:
	add %sp, $2
	ret 1
