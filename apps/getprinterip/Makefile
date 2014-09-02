BIN = getprinterip
ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip
#CC = mips-linux-uclibc-gcc


all:
	$(CC) -D$(BUILD_VERSION) main.c -o $(BIN) 

clean:
	rm -rf $(BIN)
