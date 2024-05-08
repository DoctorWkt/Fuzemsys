install:
	mkdir -p ../build/6809/bin ../build/z80/bin
	(cd ar;     make -f Makefile.6809 clean install)
	(cd cpp;    make -f Makefile.6809 clean install)
	(cd levee;  make -f Makefile.6809 clean install)
	(cd simple; make -f Makefile.6809 clean install)
	(cd ar;     make -f Makefile.z80 clean install)
	(cd cpp;    make -f Makefile.z80 clean install)
	(cd levee;  make -f Makefile.z80 clean install)
	(cd simple; make -f Makefile.z80 clean install)
	@echo
	@echo Now set FUZIXROOT to `readlink -f ../build`/CPU
	@echo where BIN is 6809 or z80

clean:
	rm -rf ../build
	(cd ar;     make -f Makefile.6809 clean)
	(cd cpp;    make -f Makefile.6809 clean)
	(cd levee;  make -f Makefile.6809 clean)
	(cd simple; make -f Makefile.6809 clean)
	(cd ar;     make -f Makefile.z80 clean)
	(cd cpp;    make -f Makefile.z80 clean)
	(cd levee;  make -f Makefile.z80 clean)
	(cd simple; make -f Makefile.z80 clean)
