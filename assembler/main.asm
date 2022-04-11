	#include "CPU.asm"
	entry code
code:
	push $20
	call firstNtets
	;call fib
	;call firstkprimes
	;call nestedWhile
code_done:
	hlt
Program_done:
	ret 0
print:
	push %r1
	push %r0
print_0:
	;introduce var i to %r0
	mov %r0, 4(%bp) ;place argument i
	;introduce var j to %r1
	mov %r1, %r0;j = i
	out %r1
print_done:
	pop %r0
	pop %r1
	ret 2
tet:
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
tet_0:
	;introduce var i to %r0
	mov %r0, 4(%bp) ;place argument i
	cmp %r0, $5;cmp i 5
	jge tet_4
	cmp %r0, $4;cmp i 4
	jge tet_3
	mov %rr, $0
	jmp tet_done
tet_3:
	mov %rr, $1
	jmp tet_done
tet_2:
	jmp tet_1
tet_4:
		;introduce var .t2 to %r1
	mov %r1, %r0
	sub %r1, $1;.t2 = i - 1
	push %r1
		;introduce var .t1 to %r1
	call tet
	mov %r1, %rr;.t1 = call tet
		;introduce var .t5 to %r2
	mov %r2, %r0
	sub %r2, $2;.t5 = i - 2
	push %r2
		;introduce var .t4 to %r2
	call tet
	mov %r2, %rr;.t4 = call tet
		;introduce var .t8 to %r3
	mov %r3, %r0
	sub %r3, $3;.t8 = i - 3
	push %r3
		;introduce var .t7 to %r3
	call tet
	mov %r3, %rr;.t7 = call tet
		;introduce var .t10 to %r4
	mov %r4, %r0
	sub %r4, $4;.t10 = i - 4
	push %r4
		;introduce var .t9 to %r4
	call tet
	mov %r4, %rr;.t9 = call tet
		;introduce var .t6 to %r0
	mov %r0, %r3
	add %r0, %r4;.t6 = .t7 + .t9
		;introduce var .t3 to %r3
	mov %r3, %r2
	add %r3, %r0;.t3 = .t4 + .t6
		;introduce var .t0 to %r2
	mov %r2, %r1
	add %r2, %r3;.t0 = .t1 + .t3
	mov %rr, %r2
	jmp tet_done
tet_1:
tet_done:
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	ret 2
firstNtets:
	push %r2
	push %r1
	push %r0
firstNtets_0:
	;introduce var n to %r0
	mov %r0, 4(%bp) ;place argument n
	;introduce var i to %r1
	mov %r1, $1;i = 1
	jmp firstNtets_2
firstNtets_2:
	cmp %r1, %r0;cmp i n
	jge firstNtets_1
	push %r1
		;introduce var result to %r2
	call tet
	mov %r2, %rr;result = call tet
	push %r2
	call print
	mov %r1, %r1
	add %r1, $1;i = i + 1
	jmp firstNtets_2
firstNtets_1:
firstNtets_done:
	pop %r0
	pop %r1
	pop %r2
	ret 2
