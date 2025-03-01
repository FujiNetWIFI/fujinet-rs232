TARGET  = fnshare.exe
AS      = wasm -q
ASFLAGS = -0 -mt -bt=DOS
CC      = wcc -q
CFLAGS  = -0 -bt=dos -ms -I../fujicom -osh -zu $(CPPFLAGS)
LD	= wlink OPTION quiet
LDFLAGS = &
	SYSTEM dos &
	OPTION MAP &
	LIBPATH ../fujicom

CFILES  = fnshare.c netredir.c
OBJS = $(CFILES:.c=.obj) $(AFILES:.asm=.obj) ../sys/print.obj ../ncopy/fujifs.obj

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) &
	  disable 1014 &
	  name $@ &
	  file {$(OBJS)} &
	  library {fujicoms.lib clibs.lib}

fnshare.obj: fnshare.c .AUTODEPEND
	$(CC) $(CFLAGS) -nt=_INIT -nc=INIT -fo=$@ $<

.c.obj: .AUTODEPEND
        $(CC) $(CFLAGS) -fo=$@ $<
.asm.obj: .AUTODEPEND
	$(AS) $(ASFLAGS) -fo=$@ $<

clean : .SYMBOLIC
	rm -f $(TARGET) *.obj *.map *.err *.sys *.com *.exe *.o
