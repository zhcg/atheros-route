ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc

RM:=rm -rf

all:clean
	$(CC) -o arp arp.c
clean:
	$(RM) arp *.o

