	#include "CPU.asm"
	entry code
	data@ data
code:
	push $256
	call testtest
code_done:
	hlt
Program_done:
	ret 0
print:
	push %r0
print_0:
	;introduce var i to %r0
	mov %r0, 4(%bp) ;place argument i
	out %r0
print_done:
	pop %r0
	ret 2
testtest:
	push %r2
	push %r1
	push %r0
testtest_0:
	;introduce var one to %r0
	mov %r0, $1;one = 1
		;introduce var two to %r1
	mov %r1, $2;two = 2
		;introduce var three to %r2
	mov %r2, $3;three = 3
	push %r2
	push %r1
	push %r0
	call test
testtest_done:
	pop %r0
	pop %r1
	pop %r2
	ret 0
test:
	push %r2
	push %r1
	push %r0
test_0:
	;introduce var arg1 to %r0
	mov %r0, 4(%bp) ;place argument arg1
	;introduce var arg2 to %r1
	mov %r1, 6(%bp) ;place argument arg2
	;introduce var arg3 to %r2
	mov %r2, 8(%bp) ;place argument arg3
	push %r0
	call print
	push %r1
	call print
	push %r2
	call print
test_done:
	pop %r0
	pop %r1
	pop %r2
	ret 6
setBlockAllocated:
	push %r3
	push %r2
	push %r1
	push %r0
setBlockAllocated_0:
	;introduce var blkPtr to %r0
	mov %r0, 4(%bp) ;place argument blkPtr
	;introduce var size to %r1
	mov %r1, 6(%bp) ;place argument size
	push %r1
	call print
	push %r0
	call print
		;introduce var .t2 to %r2
	mov %r2, $1;.t2 = 1
		;introduce var .t3 to %r3
	mov %r3, %r0
	sub %r3, $6;.t3 = blkPtr - 6
	mov (%r3), %r2;(.t3) = .t2
		;introduce var .t4 to %r2
	mov %r2, %r1;.t4 = size
		;introduce var .t5 to %r1
	mov %r1, %r0
	sub %r1, $4;.t5 = blkPtr - 4
	mov (%r1), %r2;(.t5) = .t4
setBlockAllocated_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	ret 4
setBlockNext:
	push %r2
	push %r1
	push %r0
setBlockNext_0:
	;introduce var blkPtr to %r0
	mov %r0, 4(%bp) ;place argument blkPtr
	;introduce var next to %r1
	mov %r1, 6(%bp) ;place argument next
	;introduce var .t0 to %r2
	mov %r2, %r1;.t0 = next
		;introduce var .t1 to %r1
	mov %r1, %r0
	sub %r1, $2;.t1 = blkPtr - 2
	mov (%r1), %r2;(.t1) = .t0
setBlockNext_done:
	pop %r0
	pop %r1
	pop %r2
	ret 4
mm_init:
	push %r2
	push %r1
	push %r0
mm_init_0:
	;introduce var size to %r0
	mov %r0, 4(%bp) ;place argument size
	;introduce var dataAt to %r1
	mov %r1, $2;dataAt = 2
		;introduce var .t1 to %r2
	mov %r2, (%r1);.t1 = (dataAt)
		;introduce var blkPtr to %r1
	mov %r1, %r2
	add %r1, $6;blkPtr = .t1 + 6
		;introduce var newSize to %r2
	mov %r2, %r0
	sub %r2, $6;newSize = size - 6
	push %r2
	call print
	push %r2
	push %r1
	call setBlockAllocated
		;introduce var zero to %r0
	mov %r0, $0;zero = 0
	push %r0
	push %r1
	call setBlockNext
mm_init_done:
	pop %r0
	pop %r1
	pop %r2
	ret 2
nop
data:
