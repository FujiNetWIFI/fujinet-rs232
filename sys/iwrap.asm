_TEXT	segment word public 'CODE'

	; Macro to create an interrupt wrapper for a given C function
INTERRUPT MACRO func
	PUBLIC func&vect_
	func&vect_ PROC NEAR
	;; Push onto stack in same order Watcom __interrupt does to allow use of _chain_intr
	push	ax
	push	cx
	push	dx
	push	bx
	push	sp
	push	bp
	push	si
	push	di
	push	ds
	push	es
	push	ax
	push	ax

; Set up data segment
	push	cs
	pop	ds

	call	func

	pop	ax
	pop	ax
	pop	es
	pop	ds
	pop	di
	pop	si
	pop	bp
	pop	bx
	pop	bx
	pop	dx
	pop	cx
	pop	ax
	iret
	func&vect_ ENDP
ENDM

	extern	intf5_:near
	INTERRUPT	intf5_

_TEXT	ends

	end
