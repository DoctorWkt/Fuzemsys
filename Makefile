all:
	(cd emulators; make)

install:
	(cd emulators; make install)
	(cd include;   make install)

clean:
	(cd emulators; make clean)
