Program:
Program_0:
	#include "CPU.asm"
	entry code
	data@ data
code:
	;push $4664
	;call mm_init
	;call doublePointer
	push $4660
	call test
	hlt
Program_done:
	ret 0
#d "print"
print:
	push %r1
print_0:
	mov %r1, 4(%bp)
	out %r1
print_done:
	pop %r1
	ret 2
#d "fib"
fib:
	push %r4
	push %r3
	push %r2
	push %r1
fib_0:
	mov %r1, 4(%bp)
	mov %r2, $2
		;introduce var one to %r3
	mov %r3, $1
	cmp %r1, %r2
	jge fib_2
	mov %rr, %r1
	jmp fib_done
fib_2:
		;introduce var .t2 to %r4
	sub %r4, %r1, %r2
	push %r4
		;introduce var .t1 to %r4
	call fib
	mov %r4, %rr
		;introduce var .t4 to %r2
	sub %r2, %r1, %r3
	push %r2
		;introduce var .t3 to %r2
	call fib
	mov %r2, %rr
		;introduce var .t0 to %r1
	add %r1, %r4, %r2
	mov %rr, %r1
	jmp fib_done
fib_1:
fib_done:
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	ret 2
#d "test"
test:
	push %r3
	push %r2
	push %r1
test_0:
	mov %r1, $0
		;introduce var one to %r2
	mov %r2, $1
	jmp test_2
test_2:
	cmpi %r1, $20
	jge test_1
		;introduce var result to %r3
	push %r1
	call fib
	mov %r3, %rr
	push %r3
	call print
	add %r1, %r1, %r2
	jmp test_2
test_1:
test_done:
	pop %r1
	pop %r2
	pop %r3
	ret 0
data:
