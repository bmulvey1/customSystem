	#include "CPU.asm"
	entry code
code:
	push $20
	call firstnfibs
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
firstnfibs:
	sub %sp, $14
	push %r2
	push %r1
	push %r0
firstnfibs_0:
	;introduce var n to %r0
	mov %r0, 4(%bp) ;place argument n
	;introduce var fibarr to %r1
	mov %r1, $512;fibarr = 512
		;introduce var .t0 to %r2
	mov %r2, $0;.t0 = 0
	mov (%r1), %r2;(fibarr) = .t0
		;introduce var .t1 to %r2
	mov %r2, $1;.t1 = 1
	mov -2(%bp), %r2;spill          .t1
		;introduce var .t2 to %r2
	mov %r2, %r1
	add %r2, $2;.t2 = fibarr + 2
	mov -4(%bp), %r0;spill            n
	mov %r0, -2(%bp);unspill      .t1
	mov (%r2), %r0;(.t2) = .t1
		;introduce var j to %r2
		;introduce var x to %r0
	mov %r0, $1;x = 1
	mov -2(%bp), %r2;spill            j
		;introduce var y to %r2
	mov %r2, $2;y = 2
	mov -6(%bp), %r0;spill            x
		;introduce var z to %r0
	mov %r0, $3;z = 3
	mov -8(%bp), %r1;spill       fibarr
		;introduce var i to %r1
	mov %r1, $2;i = 2
	jmp firstnfibs_2
firstnfibs_2:
	cmp %r1, -4(%bp);cmp i n
	jg firstnfibs_1
	mov -10(%bp), %r2;spill            y
		;introduce var .t7 to %r2
	mov %r2, %r1
	sub %r2, $1;.t7 = i - 1
	mov -12(%bp), %r0;spill            z
		;introduce var .t6 to %r0
	mov %r0, %r2
	mul %r0, $2;.t6 = .t7 * 2
		;introduce var .t5 to %r2
	mov %r2, -8(%bp)
	add %r2, %r0;.t5 = fibarr + .t6
		;introduce var .t4 to %r0
	mov %r0, (%r2);.t4 = (.t5)
		;introduce var .t11 to %r2
	mov %r2, %r1
	sub %r2, $2;.t11 = i - 2
	mov -14(%bp), %r0;spill          .t4
		;introduce var .t10 to %r0
	mov %r0, %r2
	mul %r0, $2;.t10 = .t11 * 2
		;introduce var .t9 to %r2
	mov %r2, -8(%bp)
	add %r2, %r0;.t9 = fibarr + .t10
		;introduce var .t8 to %r0
	mov %r0, (%r2);.t8 = (.t9)
	mov %r2, -2(%bp);unspill        j
	mov %r2, -14(%bp)
	add %r2, %r0;j = .t4 + .t8
	mov %r0, -6(%bp);unspill        x
	mov %r0, %r0
	add %r0, $1;x = x + 1
	mov -2(%bp), %r2;spill            j
	mov %r2, -10(%bp);unspill        y
	mov %r2, %r2
	add %r2, $1;y = y + 1
	mov -6(%bp), %r1;spill            i
		;introduce var .t15 to %r1
	mov %r1, %r0
	add %r1, %r2;.t15 = x + y
	push %r1
	call print
		;introduce var .t16 to %r1
	mov %r1, -2(%bp);.t16 = j
	mov -10(%bp), %r1;spill         .t16
		;introduce var .t18 to %r1
	mov %r1, -6(%bp)
	mul %r1, $2;.t18 = i * 2
	mov -14(%bp), %r0;spill            x
		;introduce var .t17 to %r0
	mov %r0, -8(%bp)
	add %r0, %r1;.t17 = fibarr + .t18
	mov %r1, -10(%bp);unspill     .t16
	mov (%r0), %r1;(.t17) = .t16
	mov %r0, -6(%bp);unspill        i
	mov %r0, %r0
	add %r0, $1;i = i + 1
	push %r0
	mov %rr, -14(%bp)
	push %rr
	mov %rr, -12(%bp)
	push %rr
	pop %r0
	pop %rr
	mov -6(%bp), %rr
	pop %r1
	jmp firstnfibs_2
firstnfibs_1:
	mov -10(%bp), %r2;spill            y
	mov %r2, -6(%bp);unspill        x
	mov %r2, %r2
	add %r2, $1;x = x + 1
	mov -6(%bp), %r2;spill            x
	mov %r2, -10(%bp);unspill        y
	mov %r2, %r2
	add %r2, $1;y = y + 1
	mov %r0, %r0
	add %r0, $1;z = z + 1
	mov %r1, $0;i = 0
	jmp firstnfibs_4
firstnfibs_4:
	cmp %r1, -4(%bp);cmp i n
	jg firstnfibs_3
	mov -2(%bp), %r2;spill            y
		;introduce var .t26 to %r2
	mov %r2, %r1
	mul %r2, $2;.t26 = i * 2
	mov -10(%bp), %r0;spill            z
		;introduce var .t25 to %r0
	mov %r0, -8(%bp)
	add %r0, %r2;.t25 = fibarr + .t26
		;introduce var .t24 to %r2
	mov %r2, (%r0);.t24 = (.t25)
	push %r2
	call print
	mov %r1, %r1
	add %r1, $1;i = i + 1
	mov %rr, -10(%bp)
	push %rr
	mov %rr, -2(%bp)
	push %rr
	pop %r2
	pop %r0
	jmp firstnfibs_4
firstnfibs_3:
	mov -2(%bp), %r2;spill            y
	mov %r2, -6(%bp);unspill        x
	push %r2
	call print
	mov %r2, -2(%bp);unspill        y
	push %r2
	call print
	push %r0
	call print
	push %r1
	call print
firstnfibs_done:
	pop %r0
	pop %r1
	pop %r2
	add %sp, $14
	ret 2
