BIN = getprinterip
ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip
#CC = mips-linux-uclibc-gcc


all:
	$(CC) main.c -o $(BIN) 

clean:
	rm -rf $(BIN)
