_TEXT	segment word public 'CODE'
	extern	intf5_:near

	; Macro to create an interrupt wrapper for a given C function
INTERRUPT MACRO func
	PUBLIC func&vect_
	func&vect_ PROC NEAR
	push	bx
	push	cx
	push	dx
	push	si
	push	di
	push	bp
	push	ds
	push	es

; Set up data segment
	push	cs
	pop	ds

	call	func

	pop	es
	pop	ds
	pop	bp
	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	bx
	iret
	func&vect_ ENDP
ENDM

	INTERRUPT	intf5_

_TEXT	ends

	end
