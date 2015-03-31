# Shinobi	Software VPN
# ©2011 YUICHIRO NAKADA

DEST=

CC=		gcc
CFLAGS=		-Os -DSHINO_SSL
LDFLAGS=	-lssl -lcrypto

# http://www.slproweb.com/products/Win32OpenSSL.html
WCC=		wine c:\\lcc\\bin\\lc
WCFLAGS=	-I. -O -DSHINO_WINDOWS -DSHINO_SSL
WLDFLAGS=	ssleay32.lib
#WLDFLAGS=	ssleay32.lib libeay32.lib
#WCC=		wine c:\\lcc\\bin\\lcc -O -DSHINO_WINDOWS
#WLD=		wine c:\\lcc\\bin\\lcclnk
##wine c:\\lcc\\bin\\pedump /exp ssleay32.dll > ssleay32.exp
#wine c:\\lcc\\bin\\pedump /exp ssleay32.lib.vc >ssleay32.exp
#wine c:\\lcc\\bin\\buildlib ssleay32.exp ssleay32.lib

all: shinobi shinobi_hub shinobi_exe shinobi_hub_exe

shinobi: shinobi.c shinobi_socket.c utils.c ssl.c ssl.h shinobi.h
	$(CC) $(CFLAGS) $(LDFLAGS) shinobi.c shinobi_socket.c utils.c ssl.c -o shinobi

shinobi_exe: shinobi.c shinobi_socket.c utils.c ssl.c ssl.h shinobi.h
	$(WCC) $(WCFLAGS) shinobi.c shinobi_socket.c utils.c ssl.c $(WLDFLAGS) -o shinobi.exe

shinobi_hub: shinobi_hub.c utils.c ssl.c ssl.h shinobi.h
	$(CC) $(CFLAGS) $(LDFLAGS) shinobi_hub.c utils.c ssl.c -o shinobi_hub

shinobi_hub_exe: shinobi_hub.c utils.c ssl.c ssl.h shinobi.h
	$(WCC) $(WCFLAGS) shinobi_hub.c utils.c ssl.c $(WLDFLAGS) -o shinobi_hub.exe
#	$(WCC) shinobi_hub.c
#	$(WCC) utils.c
#	$(WCC) ssl.c
#	$(WLD) shinobi_hub.obj utils.obj ssl.obj

install:
	mkdir -p $(DEST)/usr/bin
	install -s shinobi shinobi_hub $(DEST)/usr/bin

clean:
	rm -f shinobi shinobi_hub *.obj *.exe

rpm:
	make clean
	tar cvjf ../shinobi-0.1.tar.bz2 ../shinobi-0.1
	rpmbuild -ta ../shinobi-0.1.tar.bz2
