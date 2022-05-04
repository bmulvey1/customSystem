Program:
Program_0:
	#include "CPU.asm"
	entry code
	data@ data
code:
	;push $4664
	;call mm_init
	;call doublePointer
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
	hlt
print_done:
	pop %r1
	ret 2
#d "test"
test:
	subi %sp, %sp, $6
	push %r2
	push %r1
test_0:
	mov %r1, $1
		;introduce var j to %r2
	mov %r2, $2
		;introduce var k to %r3
	add %r3, %r1, %r2
	mov -2(%bp), %r1;spill            i
		;introduce var l to %r1
	mov -4(%bp), %r2;spill            j
	mov %r2, -2(%bp);unspill        i
	add %r1, %r2, %r2
	mov -2(%bp), %r2;spill            i
		;introduce var m to %r2
	mov -6(%bp), %r3;spill            k
	mov %r3, -4(%bp);unspill        j
	add %r2, %r3, %r3
	push $1234
	call print
	mov -4(%bp), %r3;spill            j
	mov %r3, -2(%bp);unspill        i
	push %r3
	call print
	mov %r3, -4(%bp);unspill        j
	push %r3
	call print
	mov %r3, -6(%bp);unspill        k
	push %r3
	call print
	push %r1
	call print
	push %r2
	call print
	hlt
test_done:
	pop %r1
	pop %r2
	addi %sp, %sp, $6
	ret 0
data:
