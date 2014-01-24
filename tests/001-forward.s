# Forward-declarations
;label

:main
	or 12 34 r0
	or 12 123456 r0
	or 123456 12 r0
	or 123456 123456 r0
	jmp :label
	hlt
:label
	xor r0 r0
	hlt
