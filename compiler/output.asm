fib:
	sub %sp, $0
	push %r1
	mov %r0, 4(%bp)
	cmp %r0, $2
	jge fib_1
	mov %r0, 4(%bp)
	jmp fib_done
fib_1:
	mov %r0, 4(%bp)
	sub %r0, $2
	push %r0
	call fib
	mov %r1, 4(%bp)
	sub %r1, $1
	push %r1
	mov %r1, %r0
	call fib
	add %r1, %r0
	mov %r0, %r1
	jmp fib_done
fib_done:
	pop %r1
	add %sp, $0
	ret 1
modulo:
	sub %sp, $0
modulo_1:
	mov %r0, 4(%bp)
	cmp %r0, 6(%bp)
	jl modulo_2
	mov %r0, 4(%bp)
	sub %r0, 6(%bp)
	mov 4(%bp), %r0
	jmp modulo_1
modulo_2:
	mov %r0, 4(%bp)
	jmp modulo_done
modulo_done:
	add %sp, $0
	ret 2
isPrime:
	sub %sp, $4
	push %r1
	push %r2
	mov %r0, $2
isPrime_1:
	cmp %r0, 4(%bp)
	jge isPrime_2
	push %r0
	push 4(%bp)
	mov %r1, %r0
	call modulo
	cmp %r0, $0
	jne isPrime_3
	mov %r2, $0
	mov %r0, %r2
	jmp isPrime_done
isPrime_3:
	mov %r1, %r1
	add %r1, $1
	mov %r0, -2(%bp)
	mov %r0, %r1
	jmp isPrime_1
isPrime_2:
	mov %r0, $1
	jmp isPrime_done
isPrime_done:
	pop %r2
	pop %r1
	add %sp, $4
	ret 1
firstNPrimes:
	sub %sp, $6
	push %r1
	push %r2
	mov %r0, $2
	mov %r1, $0
firstNPrimes_1:
	cmp %r1, 4(%bp)
	jge firstNPrimes_2
	push %r0
	mov %r2, %r0
	call isPrime
	cmp %r0, $1
	jne firstNPrimes_3
	mov %r1, %r1
	add %r1, $1
firstNPrimes_3:
	mov %r2, %r2
	add %r2, $1
	mov %r0, -2(%bp)
	mov %r0, %r2
	jmp firstNPrimes_1
firstNPrimes_2:
firstNPrimes_4:
	mov %r0, 4(%bp)
	cmp %r0, $0
	jle firstNPrimes_5
	push 4(%bp)
	call fib
	mov %r0, 4(%bp)
	sub %r0, $1
	mov 4(%bp), %r0
	mov -4(%bp), %r1
	jmp firstNPrimes_4
firstNPrimes_5:
firstNPrimes_done:
	pop %r2
	pop %r1
	add %sp, $6
	ret 1
