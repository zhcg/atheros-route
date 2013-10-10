# $Id: //depot/sw/releases/Aquila_9.2.0_U11/apps/nx-routed-0.99.3/Makefile#1 $

SUBDIRS = src

sbindir		= /sbin/
etcdir		= /etc
startupdir	= /etc/rc.d/init.d
mandir		= /usr/share/man
DAEMONMODE=755

all: 
	@for subdir in $(SUBDIRS); do				\
		$(MAKE) -C $$subdir || exit 1;			\
	done

clean:
	@for subdir in $(SUBDIRS); do				\
		$(MAKE) -C $$subdir clean || exit 1;	\
	done
	@rm -f core
	
distclean:	clean
	@rm -f config.cache config.status config.log src/config.h Makefile src/Makefile
	@rm -rf bin

install:
	$(STRIP) src/nx-routed
	cd src && install -m$(DAEMONMODE) nx-routed $(INSTALL_ROOT)$(sbindir)
uninstall:
	@echo "Do nothing now"
