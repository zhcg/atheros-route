BIN = remote_parse
ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip
#CC = mips-linux-uclibc-gcc


all:
	$(CC) remote_parse.c our_md5.c libmjson.a -o $(BIN) 

clean:
	rm -rf $(BIN)
