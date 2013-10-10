PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/sbin
LOCALEDIR ?= ${PREFIX}/share/locale

SRC = src
PO = po
MAN = man

CFLAGS?= -O2

all :   dnsmasq

dnsmasq :
	$(MAKE) I18N=-DNO_GETTEXT -f ../bld/Makefile -C $(SRC) dnsmasq 

clean :
	rm -f *~ $(SRC)/*.mo contrib/*/*~ */*~ $(SRC)/*.pot 
	rm -f $(SRC)/*.o $(SRC)/dnsmasq.a $(SRC)/dnsmasq core */core

install:    
	cp $(SRC)/dnsmasq $(PREFIX)/

merge :
	$(MAKE) I18N=-DLOCALEDIR='\"$(LOCALEDIR)\"' -f ../bld/Makefile -C $(SRC) dnsmasq.pot
	cd $(PO); for f in *.po; do \
		msgmerge -U $$f ../$(SRC)/dnsmasq.pot; \
	done


