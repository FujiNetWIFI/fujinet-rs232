TARGET  = ncopy.exe
AS      = wasm -q
ASFLAGS = -0 -mt -bt=DOS
CC      = wcc -q
CFLAGS  = -0 -bt=dos -ms -I../fujicom -s -osh -zu $(CPPFLAGS)
LD	= wlink OPTION quiet
LDFLAGS = &
	SYSTEM dos &
	OPTION MAP &
	LIBPATH ../fujicom

CFILES  = ncopy.c parser.c fujifs.c
OBJS = $(CFILES:.c=.obj) $(AFILES:.asm=.obj)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) &
	  disable 1014 &
	  name $@ &
	  file {$(OBJS) ../sys/print.obj} &
	  library {fujicoms.lib clibs.lib}

.c.obj: .AUTODEPEND
        $(CC) $(CFLAGS) -fo=$@ $<
.asm.obj: .AUTODEPEND
	$(AS) $(ASFLAGS) -fo=$@ $<

clean : .SYMBOLIC
	rm -f $(TARGET) *.obj *.map *.err *.sys *.com *.exe *.o
