;fact
:main
	or 0 0 r0
	or 7 7 r1
	call :fact
	hlt

;fact.skip
:fact
	cmp r1 1
	jnle :fact.skip
	or 0 1 r0
	ret
:fact.skip
	push r1
	sub r1 1 r1
	call :fact
	pop r1
	mul r0 r1
	ret
