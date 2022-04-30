Program:
Program_0:
	#include "CPU.asm"
	entry code
	data@ data
code:
	push $4664
	call mm_init
	call doublePointer
	;call test
	hlt
Program_done:
	ret 0
#d "print"
print:
	push %r0
print_0:
	;introduce var n to %r0
	mov %r0, 4(%bp) ;place argument n
	out %r0
print_done:
	pop %r0
	ret 2
#d "ptest"
ptest:
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
ptest_0:
	;introduce var p to %r0
	push $8
	call mm_malloc
	mov %r0, %rr;p = call mm_malloc
		;introduce var i to %r1
	mov %r1, $0;i = 0
	jmp ptest_2
ptest_2:
	cmp %r1, $4;cmp i 4
	jge ptest_1
	push $9
		;introduce var .t2 to %r2
	call mm_malloc
	mov %r2, %rr;.t2 = call mm_malloc
	mov %r1(%r0, $2), %r2;(p + i * 2) = .t2
		;introduce var .t4 to %r2
	mov %r2, %r1
	add %r2, $48;.t4 = i + 48
		;introduce var .t6 to %r3
	mov %r3, %r1(%r0, 2)
		;introduce var .t5 to %r4
	mov %r4, (%r3);.t5 = (.t6)
	mov (%r4), %r2;(.t5) = .t4
	mov %r1, %r1
	add %r1, $1;i = i + 1
	jmp ptest_2
ptest_1:
ptest_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	ret 0
#d "doublePointer"
doublePointer:
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
doublePointer_0:
	;introduce var array to %r0
	push $10
	call mm_malloc
	mov %r0, %rr;array = call mm_malloc
	push %r0
	call print
		;introduce var i to %r1
	mov %r1, $0;i = 0
	jmp doublePointer_2
doublePointer_2:
	cmp %r1, $5;cmp i 5
	jge doublePointer_1
	push $16
	call mm_malloc
		;introduce var thisFibArray to %r2
	push $40
	call mm_malloc
	mov %r2, %rr;thisFibArray = call mm_malloc
	push %r2
	push $20
	call firstnfibs
		;introduce var .t5 to %r3
	mov %r3, %r2;.t5 = thisFibArray
	mov %r1(%r0, $2), %r3;(array + i * 2) = .t5
	mov %r1, %r1
	add %r1, $1;i = i + 1
	jmp doublePointer_2
doublePointer_1:
	mov %r1, $0;i = 0
	jmp doublePointer_4
doublePointer_4:
	cmp %r1, $5;cmp i 5
	jge doublePointer_3
		;introduce var j to %r2
	mov %r2, $0;j = 0
		;introduce var .t8 to %r3
	mov %r3, %r1(%r0, 2)
	push %r3
	call print
	jmp doublePointer_6
doublePointer_6:
	cmp %r2, $20;cmp j 20
	jg doublePointer_5
		;introduce var .t11 to %r3
	mov %r3, %r1(%r0, 2)
		;introduce var .t10 to %r4
	mov %r4, %r2(%r3, 2)
	push %r4
	call print
	mov %r2, %r2
	add %r2, $1;j = j + 1
	jmp doublePointer_6
doublePointer_5:
	mov %r1, %r1
	add %r1, $1;i = i + 1
	jmp doublePointer_4
doublePointer_3:
doublePointer_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	ret 0
#d "firstnfibs"
firstnfibs:
	push %r5
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
firstnfibs_0:
	;introduce var n to %r0
	mov %r0, 4(%bp) ;place argument n
	;introduce var dest to %r1
	mov %r1, 6(%bp) ;place argument dest
	;introduce var fibarr to %r2
	mov %r2, %r1;fibarr = dest
		;introduce var .t0 to %r1
	mov %r1, $0;.t0 = 0
		;introduce var .t1 to %r3
	mov %r3, (%r2);.t1 = (fibarr)
	mov (%r3), %r1;(.t1) = .t0
		;introduce var .t2 to %r1
	mov %r1, $1;.t2 = 1
	mov 2(%r2), %r1;fibarr + 2 = .t2
		;introduce var i to %r1
	mov %r1, $2;i = 2
	jmp firstnfibs_2
firstnfibs_2:
	cmp %r1, %r0;cmp i n
	jg firstnfibs_1
		;introduce var .t5 to %r3
	mov %r3, %r1
	sub %r3, $1;.t5 = i - 1
		;introduce var .t4 to %r4
	mov %r4, %r3(%r2, 2)
		;introduce var .t7 to %r3
	mov %r3, %r1
	sub %r3, $2;.t7 = i - 2
		;introduce var .t6 to %r5
	mov %r5, %r3(%r2, 2)
		;introduce var .t8 to %r3
	mov %r3, %r4
	add %r3, %r5;.t8 = .t4 + .t6
	mov %r1(%r2, $2), %r3;(fibarr + i * 2) = .t8
	mov %r1, %r1
	add %r1, $1;i = i + 1
	jmp firstnfibs_2
firstnfibs_1:
firstnfibs_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	ret 4
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
	mov -2(%r0), %r3;blkPtr + -2 = .t0
		;introduce var .t1 to %r3
	mov %r3, %r1;.t1 = size
	mov -4(%r0), %r3;blkPtr + -4 = .t1
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
	;introduce var .t0 to %r1
	mov %r1, -2(%r0)
	mov %rr, %r1
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
	;introduce var .t0 to %r1
	mov %r1, -4(%r0)
	mov %rr, %r1
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
	;introduce var .t2 to %r1
	mov %r1, -4(%r0)
		;introduce var .t1 to %r2
	mov %r2, %r1
	add %r2, $4;.t1 = .t2 + 4
		;introduce var .t0 to %r1
	mov %r1, %r0
	add %r1, %r2;.t0 = blkPtr + .t1
	mov %rr, %r1
	jmp getBlockNext_done
getBlockNext_done:
	pop %r0
	pop %r1
	pop %r2
	ret 2
#d "mm_init"
mm_init:
	push %r3
	push %r2
	push %r1
	push %r0
mm_init_0:
	;introduce var size to %r0
	mov %r0, 4(%bp) ;place argument size
	;introduce var dataAt to %r1
	mov %r1, $2;dataAt = 2
		;introduce var blkPtr to %r2
		;introduce var .t1 to %r3
	mov %r3, (%r1);.t1 = (dataAt)
	mov %r2, %r3
	add %r2, $4;blkPtr = .t1 + 4
		;introduce var newSize to %r3
	mov %r3, %r0
	sub %r3, $4;newSize = size - 4
	push $0
	push %r3
	push %r2
	call setBlockAllocated
	mov %rr, $0
	jmp mm_init_done
mm_init_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	ret 2
#d "mm_malloc"
mm_malloc:
	sub %sp, $2
	push %r6
	push %r5
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
mm_malloc_0:
	;introduce var size to %r0
	mov %r0, 4(%bp) ;place argument size
	push %r0
	call print
		;introduce var dataAt to %r1
	mov %r1, $2;dataAt = 2
		;introduce var blkRunner to %r2
		;introduce var .t2 to %r3
	mov %r3, (%r1);.t2 = (dataAt)
	mov %r2, %r3
	add %r2, $4;blkRunner = .t2 + 4
		;introduce var one to %r3
	mov %r3, $1;one = 1
		;introduce var isa to %r1
		;introduce var siz to %r4
	jmp mm_malloc_2
mm_malloc_2:
	cmp %r3, $1;cmp one 1
	jne mm_malloc_1
	push %r2
	call getBlockIsAllocated
	mov %r1, %rr;isa = call getBlockIsAllocated
	push %r2
	call getBlockSize
	mov %r4, %rr;siz = call getBlockSize
	push $1234
	call print
	push %r1
	call print
	push %r4
	call print
	push %r2
		;introduce var .t8 to %r5
	call getBlockIsAllocated
	mov %r5, %rr;.t8 = call getBlockIsAllocated
	cmp %r5, $0;cmp .t8 0
	jne mm_malloc_6
	push %r2
		;introduce var .t9 to %r5
	call getBlockSize
	mov %r5, %rr;.t9 = call getBlockSize
	cmp %r5, %r0;cmp .t9 size
	jl mm_malloc_5
		;introduce var newSize to %r5
	push %r2
		;introduce var .t12 to %r6
	call getBlockSize
	mov %r6, %rr;.t12 = call getBlockSize
	mov -2(%bp), %r4;spill          siz
		;introduce var .t11 to %r4
	mov %r4, %r6
	sub %r4, %r0;.t11 = .t12 - size
	mov %r5, %r4
	sub %r5, $4;newSize = .t11 - 4
	push $1
	push %r0
	push %r2
	call setBlockAllocated
		;introduce var newBlk to %r4
		;introduce var .t15 to %r6
	mov %r6, %r0
	add %r6, $4;.t15 = size + 4
	mov %r4, %r2
	add %r4, %r6;newBlk = blkRunner + .t15
	push $0
	push %r5
	push %r4
	call setBlockAllocated
	push $0
	call print
	mov %rr, %r2
	jmp mm_malloc_done
mm_malloc_5:
	push $9999
	call print
	hlt
	push %r2
	call getBlockNext
	mov %r2, %rr;blkRunner = call getBlockNext
	jmp mm_malloc_4
mm_malloc_4:
	jmp mm_malloc_3
mm_malloc_6:
	push %r2
	call print
	push %r2
	call getBlockNext
	mov %r2, %rr;blkRunner = call getBlockNext
	jmp mm_malloc_3
mm_malloc_3:
	jmp mm_malloc_2
mm_malloc_1:
	#d "end_text"
mm_malloc_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	pop %r6
	add %sp, $2
	ret 2
data:
