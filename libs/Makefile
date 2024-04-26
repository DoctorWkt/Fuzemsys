all:
	@echo "Use one of the Makefile.* files to do a make"
	@echo "Or do 'make install' or 'make clean'"

install:
	make -f Makefile.6809 install
	make -f Makefile.6809 clean
	make -f Makefile.z80 install
	make -f Makefile.z80 clean

clean:
	make -f Makefile.6809 clean
	make -f Makefile.z80 clean
