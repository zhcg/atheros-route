ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
CFLAGS:=-Wall -Wno-unused -g -I.
RM:=rm -rf

all:clean
	$(CC) $(CFLAGS) -o get_mac get_mac.c
	$(CC) $(CFLAGS) -o set_mac set_mac.c
	$(CC) $(CFLAGS) -o set_macaddr set_macaddr.c
clean:
	$(RM) *.o get_mac set_mac set_macaddr

