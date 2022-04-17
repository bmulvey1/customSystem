	#include "CPU.asm"
	entry code
	data@ data
code:
	push $32768
	call mm_init
	push $235
	call malloc_test
code_done:
	hlt
Program_done:
	ret 0
#d "print"
print:
	push %r0
print_0:
	;introduce var i to %r0
	mov %r0, 4(%bp) ;place argument i
	out %r0
print_done:
	pop %r0
	ret 2
#d "malloc_test"
malloc_test:
	push %r2
	push %r1
	push %r0
malloc_test_0:
	;introduce var n to %r0
	mov %r0, 4(%bp) ;place argument n
	;introduce var i to %r1
	mov %r1, $2;i = 2
		;introduce var result to %r2
	mov %r2, $0;result = 0
	jmp malloc_test_2
malloc_test_2:
	cmp %r1, %r0;cmp i n
	jge malloc_test_1
	push %r1
	call mm_malloc
	mov %r2, %rr;result = call mm_malloc
	mov %r1, %r1
	add %r1, $1;i = i + 1
	jmp malloc_test_2
malloc_test_1:
malloc_test_done:
	pop %r0
	pop %r1
	pop %r2
	ret 2
#d "setBlockAllocated"
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
	;introduce var isAllocated to %r2
	mov %r2, 8(%bp) ;place argument isAllocated
	;introduce var .t0 to %r3
	mov %r3, %r2;.t0 = isAllocated
		;introduce var .t1 to %r2
	mov %r2, %r0
	sub %r2, $2;.t1 = blkPtr - 2
	mov (%r2), %r3;(.t1) = .t0
		;introduce var .t2 to %r3
	mov %r3, %r1;.t2 = size
		;introduce var .t3 to %r1
	mov %r1, %r0
	sub %r1, $4;.t3 = blkPtr - 4
	mov (%r1), %r3;(.t3) = .t2
setBlockAllocated_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	ret 6
#d "getBlockIsAllocated"
getBlockIsAllocated:
	push %r1
	push %r0
getBlockIsAllocated_0:
	;introduce var blkPtr to %r0
	mov %r0, 4(%bp) ;place argument blkPtr
	;introduce var .t1 to %r1
	mov %r1, %r0
	sub %r1, $2;.t1 = blkPtr - 2
		;introduce var .t0 to %r0
	mov %r0, (%r1);.t0 = (.t1)
	mov %rr, %r0
	jmp getBlockIsAllocated_done
getBlockIsAllocated_done:
	pop %r0
	pop %r1
	ret 2
#d "getBlockSize"
getBlockSize:
	push %r1
	push %r0
getBlockSize_0:
	;introduce var blkPtr to %r0
	mov %r0, 4(%bp) ;place argument blkPtr
	;introduce var .t1 to %r1
	mov %r1, %r0
	sub %r1, $4;.t1 = blkPtr - 4
		;introduce var .t0 to %r0
	mov %r0, (%r1);.t0 = (.t1)
	mov %rr, %r0
	jmp getBlockSize_done
getBlockSize_done:
	pop %r0
	pop %r1
	ret 2
#d "getBlockNext"
getBlockNext:
	push %r2
	push %r1
	push %r0
getBlockNext_0:
	;introduce var blkPtr to %r0
	mov %r0, 4(%bp) ;place argument blkPtr
	;introduce var .t3 to %r1
	mov %r1, %r0
	sub %r1, $4;.t3 = blkPtr - 4
		;introduce var .t2 to %r2
	mov %r2, (%r1);.t2 = (.t3)
		;introduce var .t1 to %r1
	mov %r1, %r2
	add %r1, $4;.t1 = .t2 + 4
		;introduce var .t0 to %r2
	mov %r2, %r0
	add %r2, %r1;.t0 = blkPtr + .t1
	mov %rr, %r2
	jmp getBlockNext_done
getBlockNext_done:
	pop %r0
	pop %r1
	pop %r2
	ret 2
#d "mm_init"
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
	add %r1, $4;blkPtr = .t1 + 4
		;introduce var newSize to %r2
	mov %r2, %r0
	sub %r2, $4;newSize = size - 4
	push %r2
	call print
	push $0
	push %r2
	push %r1
	call setBlockAllocated
	push %r1
	call print
	mov %rr, $0
	jmp mm_init_done
mm_init_done:
	pop %r0
	pop %r1
	pop %r2
	ret 2
#d "mm_malloc"
mm_malloc:
	push %r5
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
mm_malloc_0:
	;introduce var size to %r0
	mov %r0, 4(%bp) ;place argument size
	;introduce var dataAt to %r1
	mov %r1, $2;dataAt = 2
		;introduce var .t1 to %r2
	mov %r2, (%r1);.t1 = (dataAt)
		;introduce var blkRunner to %r1
	mov %r1, %r2
	add %r1, $4;blkRunner = .t1 + 4
		;introduce var one to %r2
	mov %r2, $1;one = 1
	jmp mm_malloc_2
mm_malloc_2:
	cmp %r2, $1;cmp one 1
	jne mm_malloc_1
	push %r1
		;introduce var .t2 to %r3
	call getBlockIsAllocated
	mov %r3, %rr;.t2 = call getBlockIsAllocated
	cmp %r3, $0;cmp .t2 0
	jne mm_malloc_6
	push %r1
		;introduce var .t3 to %r3
	call getBlockSize
	mov %r3, %rr;.t3 = call getBlockSize
	cmp %r3, %r0;cmp .t3 size
	jl mm_malloc_5
	push %r1
		;introduce var .t6 to %r3
	call getBlockSize
	mov %r3, %rr;.t6 = call getBlockSize
		;introduce var .t5 to %r4
	mov %r4, %r3
	sub %r4, %r0;.t5 = .t6 - size
		;introduce var newSize to %r3
	mov %r3, %r4
	sub %r3, $4;newSize = .t5 - 4
	push $1
	push %r0
	push %r1
	call setBlockAllocated
		;introduce var .t9 to %r4
	mov %r4, %r0
	add %r4, $4;.t9 = size + 4
		;introduce var newBlk to %r5
	mov %r5, %r1
	add %r5, %r4;newBlk = blkRunner + .t9
	push $0
	push %r3
	push %r5
	call setBlockAllocated
	mov %rr, %r1
	jmp mm_malloc_done
mm_malloc_5:
	hlt
	push %r1
	call getBlockNext
	mov %r1, %rr;blkRunner = call getBlockNext
	jmp mm_malloc_4
mm_malloc_4:
	jmp mm_malloc_3
mm_malloc_6:
	push %r1
	call getBlockNext
	mov %r1, %rr;blkRunner = call getBlockNext
	jmp mm_malloc_3
mm_malloc_3:
	jmp mm_malloc_2
mm_malloc_1:
mm_malloc_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	ret 2
data:
