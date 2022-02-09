#include "CPU.asm"
entry code

code:
	push $20
	call firstnfibs
code_done:
	hlt

firstnfibs:
	sub %sp, $6
	push %r1
	push %r2

	mov %r0, $1
			;i = 1                   

firstnfibs_1:

	cmp %r0, 4(%bp)
			;cmp i n                 
	jg firstnfibs_2
			;jg label 2              
	mov %r1, %r0
	sub %r1, $1
			;.t0 = i - 1             
	mov %r2, 4(%bp)
	sub %r2, %r1
			;j = n - .t0             
	push %r0
			;push i                  
	mov %r1, %r0
	call fib
	out %r0

	push %r2
			;push j                  
	call fib
	out %r0

	mov %r1, %r1
	add %r1, $1
			;i = i + 1 - Reorderable 
	mov %r0, -2(%bp)
	mov %r0, %r1
	mov -4(%bp), %r2

	jmp firstnfibs_1
			;jmp label 1             
firstnfibs_2:


firstnfibs_done:
	pop %r2
	pop %r1
	add %sp, $6
	ret 1


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
