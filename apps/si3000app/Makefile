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

ALL=	pcmtest_send pcmtest_rece pcmtest_ioctl

all:	${ALL}

pcmtest_ioctl: pcmtest_ioctl.c
	${CC} -o pcmtest_ioctl pcmtest_ioctl.c
pcmtest_rece: pcmtest_rece.c
	${CC} -o pcmtest_rece pcmtest_rece.c
pcmtest_send: pcmtest_send.c
	${CC} -o pcmtest_send pcmtest_send.c

clean: 
	rm -f ${ALL} 
