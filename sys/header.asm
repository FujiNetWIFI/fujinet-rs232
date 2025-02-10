DGROUP  group   _SYS_HEADER, _TEXT, _INIT

_INIT   segment word public 'INIT'

	public	_config_env
        public  _driver_end
        public  _small_code_

; Pre-allocate space for converting the CONFIG.SYS parameters to environ
_config_env label near
	db 256 dup(?)

_driver_end label near
_small_code_    dw      ?

_INIT   ends

_SYS_HEADER segment word public 'SYS_HEADER'

        org     0

_sys_hdr_ label near

	extrn	Strategy_:near
	extrn	Interrupt_:near

	dd	-1
	dw	2002h
	dw	Strategy_
	dw	Interrupt_
	db	8, 0, 0, 0, 0, 0, 0, 0

_SYS_HEADER ends

        end     _sys_hdr_
