Program:
Program_0:
	#include "CPU.asm"
	entry code
	data@ data
code:
	push $4664
	call mm_init
	call doublePointer
	;push $4660
	;call test
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
#d "ptest"
ptest:
	push %r5
	push %r4
	push %r3
	push %r2
	push %r1
ptest_0:
	push $8
	call mm_malloc
	mov %r1, %rr
		;introduce var i to %r2
	mov %r2, $0
	jmp ptest_2
ptest_2:
	cmpi %r2, $4
	jge ptest_1
	push $9
		;introduce var .t2 to %r3
	call mm_malloc
	mov %r3, %rr
	mov %r2(%r1, $2), %r3
		;introduce var .t4 to %r3
	addi %r3, %r2, $48
		;introduce var .t6 to %r4
	mov %r4, %r2(%r1, $2)
		;introduce var .t5 to %r5
	mov %r5, (%r4)
	mov (%r5), %r3
	addi %r2, %r2, $1
	jmp ptest_2
ptest_1:
ptest_done:
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	ret 0
#d "doublePointer"
doublePointer:
	push %r5
	push %r4
	push %r3
	push %r2
	push %r1
doublePointer_0:
	push $10
	call mm_malloc
	mov %r1, %rr
	push %r1
	call print
		;introduce var i to %r2
	mov %r2, $0
	jmp doublePointer_2
doublePointer_2:
	cmpi %r2, $5
	jge doublePointer_1
	push $16
	call mm_malloc
		;introduce var thisFibArray to %r3
	push $42
	call mm_malloc
	mov %r3, %rr
	push %r3
	push $20
	call firstnfibs
		;introduce var .t5 to %r4
	mov %r4, %r3
	mov %r2(%r1, $2), %r4
	addi %r2, %r2, $1
	jmp doublePointer_2
doublePointer_1:
	mov %r2, $0
	jmp doublePointer_4
doublePointer_4:
	cmpi %r2, $5
	jge doublePointer_3
		;introduce var j to %r3
	mov %r3, $0
	jmp doublePointer_6
doublePointer_6:
	cmpi %r3, $20
	jg doublePointer_5
		;introduce var .t9 to %r4
	mov %r4, %r2(%r1, $2)
		;introduce var .t8 to %r5
	mov %r5, %r3(%r4, $2)
	push %r5
	call print
	addi %r3, %r3, $1
	jmp doublePointer_6
doublePointer_5:
	addi %r2, %r2, $1
	jmp doublePointer_4
doublePointer_3:
doublePointer_done:
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	ret 0
#d "firstnfibs"
firstnfibs:
	push %r6
	push %r5
	push %r4
	push %r3
	push %r2
	push %r1
firstnfibs_0:
	mov %r1, 4(%bp)
	mov %r2, 6(%bp)
	mov %r3, %r2
		;introduce var .t0 to %r2
	mov %r2, $0
		;introduce var .t1 to %r4
	mov %r4, (%r3)
	mov (%r4), %r2
		;introduce var .t2 to %r2
	mov %r2, $1
	mov 2(%r3), %r2
		;introduce var i to %r2
	mov %r2, $2
	jmp firstnfibs_2
firstnfibs_2:
	cmp %r2, %r1
	jg firstnfibs_1
		;introduce var .t5 to %r4
	subi %r4, %r2, $1
		;introduce var .t4 to %r5
	mov %r5, %r4(%r3, $2)
		;introduce var .t7 to %r4
	subi %r4, %r2, $2
		;introduce var .t6 to %r6
	mov %r6, %r4(%r3, $2)
		;introduce var .t8 to %r4
	add %r4, %r5, %r6
	mov %r2(%r3, $2), %r4
	addi %r2, %r2, $1
	jmp firstnfibs_2
firstnfibs_1:
firstnfibs_done:
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	pop %r6
	ret 4
#d "setBlockAllocated"
setBlockAllocated:
	push %r4
	push %r3
	push %r2
	push %r1
setBlockAllocated_0:
	mov %r1, 4(%bp)
	mov %r2, 6(%bp)
	mov %r3, 8(%bp)
	mov %r4, %r3
	mov -2(%r1), %r4
		;introduce var .t1 to %r4
	mov %r4, %r2
	mov -4(%r1), %r4
setBlockAllocated_done:
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	ret 6
#d "getBlockIsAllocated"
getBlockIsAllocated:
	push %r2
	push %r1
getBlockIsAllocated_0:
	mov %r1, 4(%bp)
	mov %r2, -2(%r1)
	mov %rr, %r2
	jmp getBlockIsAllocated_done
getBlockIsAllocated_done:
	pop %r1
	pop %r2
	ret 2
#d "getBlockSize"
getBlockSize:
	push %r2
	push %r1
getBlockSize_0:
	mov %r1, 4(%bp)
	mov %r2, -4(%r1)
	mov %rr, %r2
	jmp getBlockSize_done
getBlockSize_done:
	pop %r1
	pop %r2
	ret 2
#d "getBlockNext"
getBlockNext:
	push %r3
	push %r2
	push %r1
getBlockNext_0:
	mov %r1, 4(%bp)
	mov %r2, -4(%r1)
		;introduce var .t1 to %r3
	addi %r3, %r2, $4
		;introduce var .t0 to %r2
	add %r2, %r1, %r3
	mov %rr, %r2
	jmp getBlockNext_done
getBlockNext_done:
	pop %r1
	pop %r2
	pop %r3
	ret 2
#d "mm_init"
mm_init:
	push %r4
	push %r3
	push %r2
	push %r1
mm_init_0:
	mov %r1, 4(%bp)
	mov %r2, $2
		;introduce var blkPtr to %r3
		;introduce var .t1 to %r4
	mov %r4, (%r2)
	addi %r3, %r4, $4
		;introduce var newSize to %r4
	subi %r4, %r1, $4
	push $0
	push %r4
	push %r3
	call setBlockAllocated
	mov %rr, $0
	jmp mm_init_done
mm_init_done:
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	ret 2
#d "mm_malloc"
mm_malloc:
	push %r6
	push %r5
	push %r4
	push %r3
	push %r2
	push %r1
mm_malloc_0:
	mov %r1, 4(%bp)
	push %r1
	call print
		;introduce var dataAt to %r2
	mov %r2, $2
		;introduce var blkRunner to %r3
		;introduce var .t2 to %r4
	mov %r4, (%r2)
	addi %r3, %r4, $4
		;introduce var one to %r4
	mov %r4, $1
	jmp mm_malloc_2
mm_malloc_2:
	cmpi %r4, $1
	jne mm_malloc_1
	push %r3
		;introduce var .t3 to %r2
	call getBlockIsAllocated
	mov %r2, %rr
	cmpi %r2, $0
	jne mm_malloc_6
	push %r3
		;introduce var .t4 to %r2
	call getBlockSize
	mov %r2, %rr
	cmp %r2, %r1
	jl mm_malloc_5
		;introduce var newSize to %r2
	push %r3
		;introduce var .t7 to %r5
	call getBlockSize
	mov %r5, %rr
		;introduce var .t6 to %r6
	sub %r6, %r5, %r1
	subi %r2, %r6, $4
	push $1
	push %r1
	push %r3
	call setBlockAllocated
		;introduce var newBlk to %r5
		;introduce var .t10 to %r6
	addi %r6, %r1, $4
	add %r5, %r3, %r6
	push $0
	push %r2
	push %r5
	call setBlockAllocated
	mov %rr, %r3
	jmp mm_malloc_done
mm_malloc_5:
	push $9999
	call print
	hlt
	push %r3
	call getBlockNext
	mov %r3, %rr
	jmp mm_malloc_4
mm_malloc_4:
	jmp mm_malloc_3
mm_malloc_6:
	push %r3
	call getBlockNext
	mov %r3, %rr
	jmp mm_malloc_3
mm_malloc_3:
	jmp mm_malloc_2
mm_malloc_1:
	#d "end_text"
mm_malloc_done:
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	pop %r6
	ret 2
data:
