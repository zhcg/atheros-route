#
# Makefile for Atheros tools.
#

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP           =$(CROSS)strip

ifeq ($(strip ${ATH_WASP_I2S}),1)
EXTRA_CFLAGS += -DWASP
endif

ALL=	server

all:	${ALL}

server: server.c
	${CC} -o server server.c 


clean: 
	rm -f ${ALL} 
