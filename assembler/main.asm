	#include "CPU.asm"
	entry code
code:
	push $20
	;call firstNtets
	;call fib
	call firstkprimes
	;call nestedWhile
code_done:
	hlt
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
	pop %r0
	pop %r1
	ret 2
firstkprimes:
	push %r5
	push %r4
	push %r3
	push %r2
	push %r1
	push %r0
firstkprimes_0:
	;introduce var primes to %r0
	mov %r0, $1000;primes = 1000
		;introduce var i to %r1
	mov %r1, $2;i = 2
		;introduce var j to %r2
	jmp firstkprimes_2
firstkprimes_2:
	cmp %r1, $10000;cmp i 10000
	jg firstkprimes_1
		;introduce var .t2 to %r3
	mov %r3, %r1
	mul %r3, $2;.t2 = i * 2
		;introduce var .t1 to %r4
	mov %r4, %r0
	add %r4, %r3;.t1 = primes + .t2
		;introduce var .t0 to %r3
	mov %r3, (%r4);.t0 = (.t1)
	cmp %r3, $0;cmp .t0 0
	jne firstkprimes_3
	mov %r2, %r1;j = i
	jmp firstkprimes_5
firstkprimes_5:
	cmp %r2, $10000;cmp j 10000
	jg firstkprimes_4
		;introduce var .t3 to %r3
	mov %r3, $1;.t3 = 1
		;introduce var .t5 to %r4
	mov %r4, %r2
	mul %r4, $2;.t5 = j * 2
		;introduce var .t4 to %r5
	mov %r5, %r0
	add %r5, %r4;.t4 = primes + .t5
	mov (%r5), %r3;(.t4) = .t3
	mov %r2, %r2
	add %r2, %r1;j = j + i
	jmp firstkprimes_5
firstkprimes_4:
	jmp firstkprimes_3
firstkprimes_3:
	mov %r1, %r1
	add %r1, $1;i = i + 1
	jmp firstkprimes_2
firstkprimes_1:
	mov %r1, %r1;i = i
	mov %r2, %r2;j = j
		;introduce var k to %r0
	mov %r0, %r1
	add %r0, %r2;k = i + j
	pop %r0
	pop %r1
	pop %r2
	pop %r3
	pop %r4
	pop %r5
	ret 0
