TARGET  = fnshare2.exe
AS      = wasm -q
ASFLAGS = -0 -mt -bt=DOS
CC      = wcc -q
CFLAGS  = -0 -d2 -bt=dos -ms -I../fujicom -osh -zu $(CPPFLAGS)
LD	= wlink OPTION quiet
LDFLAGS = &
	SYSTEM dos &
	OPTION MAP &
	DEBUG ALL &
	LIBPATH ../fujicom

CFILES  = phantom.c bios.c dosfunc.c xms.c ramdrive.c redir.c
OBJS = $(CFILES:.c=.obj) $(AFILES:.asm=.obj) ../sys/print.obj ../ncopy/fujifs.obj

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) &
	  disable 1014 &
	  name $@ &
	  file {$(OBJS)} &
	  library {fujicoms.lib}

fnshare.obj: fnshare.c .AUTODEPEND
	$(CC) $(CFLAGS) -nt=_INIT -nc=INIT -fo=$@ $<

.c.obj: .AUTODEPEND
        $(CC) $(CFLAGS) -fo=$@ $<
.asm.obj: .AUTODEPEND
	$(AS) $(ASFLAGS) -fo=$@ $<

clean : .SYMBOLIC
	rm -f $(TARGET) *.obj *.map *.err *.sys *.com *.exe *.o
