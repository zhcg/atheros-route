ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
CFLAGS:=-Wall -Wno-unused -g -I.
RM:=rm -rf

all:clean
	$(CC) $(CFLAGS) -o sta_access_control sta_access_control.c
clean:
	$(RM) sta_access_control *.o

