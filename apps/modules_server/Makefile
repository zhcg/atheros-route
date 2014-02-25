#CC = /home/zhangbo/9344/build/gcc-4.3.3/build_mips/staging_dir/usr/bin/mips-linux-uclibc-gcc
BIN = modules_server
ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip


all:
	$(CC) modules_main.c modules_server.c as532.c as532_update.c stm32_update.c file.c -o $(BIN) -lpthread

clean:
	rm -rf $(BIN)
