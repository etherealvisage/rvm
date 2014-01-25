;test
:main
	or 0 0 r0
	call :test

	call :test

	call :test

	hlt

:test
	add r0 1
	ret
