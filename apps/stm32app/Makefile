#
# Makefile for Atheros tools.
#

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP           =$(CROSS)strip

ALL=	stm32app

all:	${ALL}

stm32app: stm32_test.c
	${CC} -o stm32app stm32_test.c

clean: 
	rm -f ${ALL} 
