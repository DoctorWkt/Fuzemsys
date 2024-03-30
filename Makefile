all:
	(cd emulators; make)
	(cd libs;      make)

install:
	(cd emulators; make install)
	(cd include;   make install)
	(cd libs;      make install)

clean:
	(cd emulators; make clean)
	(cd libs;      make clean)
