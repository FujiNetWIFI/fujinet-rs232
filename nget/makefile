AS      = wasm -q
ASFLAGS = -0 -mt -bt=DOS
CC      = wcc -q
CFLAGS  = -0 -bt=dos -ms -I../fujicom -osh $(CPPFLAGS)
LD	= wlink OPTION quiet
LDFLAGS = SYSTEM dos OPTION MAP LIBPATH ../fujicom

all: nget.exe

nget.exe: nget.obj

nput.exe: nput.obj

.obj.exe:
	$(LD) $(LDFLAGS) NAME $@ file {$< ../sys/print.obj} LIBRARY {fujicoms.lib clibs.lib}
.c.obj: .AUTODEPEND
	$(CC) $(CFLAGS) -fo=$@ $<
.asm.obj: .AUTODEPEND
	$(AS) $(ASFLAGS) -fo=$@ $<

clean : .SYMBOLIC
	rm -f $(TARGET) *.obj *.map *.err
