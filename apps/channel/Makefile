#
# Makefile for Atheros tools.
#

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
#LD=$(CROSS)ld
STRIP           =$(CROSS)strip

ifeq ($(strip ${ATH_WASP_I2S}),1)
EXTRA_CFLAGS += -DWASP
endif

ALL=	server

all:	${ALL}

server: server.c
#	$(LD) -lpthread server.o -o server
#server.o:server.c
	${CC} -pthread $^ -o $@


clean: 
	rm -f ${ALL} 
