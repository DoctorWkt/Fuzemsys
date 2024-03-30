# For now, we install only the 6809 include files
all:

clean:

install:
	mkdir -p /opt/fcc/lib/6809/include
	mkdir -p /opt/fcc/lib/6809/include/arpa
	mkdir -p /opt/fcc/lib/6809/include/net
	mkdir -p /opt/fcc/lib/6809/include/netinet
	mkdir -p /opt/fcc/lib/6809/include/readline
	mkdir -p /opt/fcc/lib/6809/include/sys
	rsync -a *.h /opt/fcc/lib/6809/include
	rsync -a arpa net netinet readline sys \
		 /opt/fcc/lib/6809/include
	rsync -a 6809/std*.h /opt/fcc/lib/6809/include
