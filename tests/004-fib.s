;fib
:main
	or 0 0 r0
	or 30 0 r1

	call :fib
	hlt

;fib.calc
:fib
	cmp r1 1
	jnle :fib.calc
	or r1 0 r0
	ret
:fib.calc
	sub r1 1 r1
	push r1
	call :fib
	pop r1
	push r0
	sub r1 1 r1
	call :fib
	pop r1
	add r0 r1 r0
	ret
