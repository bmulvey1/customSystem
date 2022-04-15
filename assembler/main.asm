	#include "CPU.asm"
	entry code
code:
	push $64
	call writeTwelve
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
writeTwelve:
	push %r1
	push %r0
writeTwelve_0:
	;introduce var destination to %r0
	mov %r0, 4(%bp) ;place argument destination
	;introduce var .t0 to %r1
	mov %r1, $12;.t0 = 12
	mov (%r0), %r1;(destination) = .t0
writeTwelve_done:
	pop %r0
	pop %r1
	ret 2
