_TEXT	segment word public 'CODE'
	public	int_wrapper_
	extern	intf5_:near

int_wrapper_:
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

	call	intf5_

	pop	es
	pop	ds
	pop	bp
	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	bx
	iret

_TEXT	ends

	end
