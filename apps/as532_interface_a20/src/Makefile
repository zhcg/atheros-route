BIN = as532_interface
SRC = as532_interface.c update.c our_md5.c
ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip
#CC = /home/zhangbo/9344/build/gcc-4.3.3/build_mips/staging_dir/usr/bin/mips-linux-uclibc-gcc
CC = arm-linux-gnueabi-gcc
AS532_FLAGS = -I ../include -I ../tools/json/include/mjson-1.5 -L ../tools/json/lib/ -lpthread -lmjson

all:
	$(CC) $(SRC) -o $(BIN) $(AS532_FLAGS) 
	@cp $(BIN) ..

clean:
	@rm -rf $(BIN) ../$(BIN)
