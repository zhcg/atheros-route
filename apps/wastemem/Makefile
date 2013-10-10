# Makefile for wastemem
#

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP           =$(CROSS)strip

ALL=	wastemem

all:	${ALL}

wastemem: wastemem.c
	${CC} -o wastemem wastemem.c

install: all
	cp -a wastemem $(INSTALL_ROOT)/sbin/.

clean: 
	rm -f ${ALL} 
