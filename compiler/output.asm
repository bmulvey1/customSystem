lessfive:
	sub %sp, $2
	mov %r0, 4(%bp)
	cmp %r0, $5
	jg lessfive_1
	mov %r0, $5
	jmp lessfive_done
lessfive_1:
	mov %r0, $6
	jmp lessfive_done
lessfive_done:
	add %sp, $2
	ret 1
