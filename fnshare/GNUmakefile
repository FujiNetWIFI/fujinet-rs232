WMAKE=wmake -h

all:
	@$(WMAKE) -e CPPFLAGS=-DVERSION='\"$(shell git rev-parse --short HEAD)$(shell git status --porcelain | grep -q '^[ MADRCU]' && echo '*')\"'

clean:
	@$(WMAKE) clean
