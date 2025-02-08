TARGET  = fujinet.sys
AS      = wasm -q
ASFLAGS = -0 -mt -bt=DOS
CC      = wcc -q
CFLAGS  = -0 -bt=dos -ms -I../fujicom -s -osh -zu $(CPPFLAGS)
LD	= wlink OPTION quiet
LDFLAGS = &
	SYSTEM dos com &
	ORDER &
	  clname SYS_HEADER &
	  clname DATA &
	  clname CODE &
	  clname BSS &
	  clname INIT &
	OPTION MAP, NODEFAULTLIBS &
	LIBPATH ../fujicom

CFILES  = commands.c dispatch.c init.c print.c intf5.c
AFILES  = header.asm iwrap.asm
OBJS = $(CFILES:.c=.obj) $(AFILES:.asm=.obj)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) &
	  disable 1014, &
	  statics &
	  name $@ &
	  file {$(OBJS)} &
	  library {fujicoms.lib clibs.lib}

init.obj: init.c .AUTODEPEND
	$(CC) $(CFLAGS) -nt=_INIT -nc=INIT -fo=$@ $<

.c.obj: .AUTODEPEND
        $(CC) $(CFLAGS) -fo=$@ $<
.asm.obj: .AUTODEPEND
	$(AS) $(ASFLAGS) -fo=$@ $<

clean : .SYMBOLIC
	rm -f $(TARGET) *.obj *.map *.err *.sys *.com *.exe
