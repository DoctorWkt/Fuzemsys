# For now, we install only the 6809 and z80 include files

CROSS6809DIR=/opt/fcc/lib/6809
CROSSZ80DIR=/opt/fcc/lib/z80
NATIVE6809DIR=../build/6809/usr
NATIVEZ80DIR=../build/z80/usr

all:

clean:

# This is for the cross-compiler use
install:
	# 6809 stuff
	mkdir -p $(CROSS6809DIR)/include
	mkdir -p $(CROSS6809DIR)/include/arpa
	mkdir -p $(CROSS6809DIR)/include/net
	mkdir -p $(CROSS6809DIR)/include/netinet
	mkdir -p $(CROSS6809DIR)/include/readline
	mkdir -p $(CROSS6809DIR)/include/sys
	rsync -a *.h $(CROSS6809DIR)/include
	rsync -a arpa net netinet readline sys \
		 $(CROSS6809DIR)/include
	rsync -a 6809/std*.h $(CROSS6809DIR)/include

	# Z80 stuff
	mkdir -p $(CROSSZ80DIR)/include
	mkdir -p $(CROSSZ80DIR)/include/arpa
	mkdir -p $(CROSSZ80DIR)/include/net
	mkdir -p $(CROSSZ80DIR)/include/netinet
	mkdir -p $(CROSSZ80DIR)/include/readline
	mkdir -p $(CROSSZ80DIR)/include/sys
	rsync -a *.h $(CROSSZ80DIR)/include
	rsync -a arpa net netinet readline sys \
		 $(CROSSZ80DIR)/include
	rsync -a z80/std*.h $(CROSSZ80DIR)/include

# This is for the (emulated) native compiler use
buildinstall:
	# 6809 stuff
	mkdir -p $(NATIVE6809DIR)/include
	mkdir -p $(NATIVE6809DIR)/include/arpa
	mkdir -p $(NATIVE6809DIR)/include/net
	mkdir -p $(NATIVE6809DIR)/include/netinet
	mkdir -p $(NATIVE6809DIR)/include/readline
	mkdir -p $(NATIVE6809DIR)/include/sys
	rsync -a *.h $(NATIVE6809DIR)/include
	rsync -a arpa net netinet readline sys \
		 $(NATIVE6809DIR)/include
	rsync -a 6809/std*.h $(NATIVE6809DIR)/include

	# Z80 stuff
	mkdir -p $(NATIVEZ80DIR)/include
	mkdir -p $(NATIVEZ80DIR)/include/arpa
	mkdir -p $(NATIVEZ80DIR)/include/net
	mkdir -p $(NATIVEZ80DIR)/include/netinet
	mkdir -p $(NATIVEZ80DIR)/include/readline
	mkdir -p $(NATIVEZ80DIR)/include/sys
	rsync -a *.h $(NATIVEZ80DIR)/include
	rsync -a arpa net netinet readline sys \
		 $(NATIVEZ80DIR)/include
	rsync -a z80/std*.h $(NATIVEZ80DIR)/include
