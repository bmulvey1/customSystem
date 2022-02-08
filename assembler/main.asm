#include "CPU.asm"
entry code

code:
	inc %r1
	cmp %r1, $20
	jg code_done
	push %r1
	call lessfive
	out %r0
	jmp code
code_done:
	hlt

lessfive:
	sub %sp, $2
	mov %r0, $7		;answer = 7
	mov %r1, 4(%bp)
	cmp %r1, $10		;cmp n 10

	jge lessfive_1		;jge label 1
	mov %r1, 4(%bp)
	cmp %r1, $9		;cmp n 9

	jge lessfive_2		;jge label 2
	mov %r1, 4(%bp)
	cmp %r1, $8		;cmp n 8

	jge lessfive_3		;jge label 3
	mov %r1, 4(%bp)
	cmp %r1, $7		;cmp n 7

	jge lessfive_4		;jge label 4
	mov %r1, 4(%bp)
	cmp %r1, $6		;cmp n 6

	jge lessfive_5		;jge label 5
	mov %r1, 4(%bp)
	cmp %r1, $5		;cmp n 5

	jge lessfive_6		;jge label 6
	mov %r0, $4		;answer = 4

	jmp lessfive_7		;jmp label 7
lessfive_6:

	mov %r0, $5		;answer = 5

lessfive_7:


	jmp lessfive_8		;jmp label 8
lessfive_5:

	mov %r0, $6		;answer = 6

lessfive_8:


	jmp lessfive_9		;jmp label 9
lessfive_4:

	mov %r0, $7		;answer = 7

lessfive_9:


	jmp lessfive_10		;jmp label 10
lessfive_3:

	mov %r0, $8		;answer = 8

lessfive_10:


	jmp lessfive_11		;jmp label 11
lessfive_2:

	mov %r0, $9		;answer = 9

lessfive_11:


	jmp lessfive_12		;jmp label 12
lessfive_1:

	mov %r0, $10		;answer = 10

lessfive_12:

		;.RETVAL = answer
	jmp lessfive_done		;ret .RETVAL
lessfive_done:
	add %sp, $2
	ret 1


