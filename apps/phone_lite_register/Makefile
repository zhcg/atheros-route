BIN = phone_lite_register
ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip
#CC = /home/zhangbo/9344/build/gcc-4.3.3/build_mips/staging_dir/usr/bin/mips-linux-uclibc-gcc
REGISTER_FLAGS = -I ./sqlite-mips-linux/include -L ./sqlite-mips-linux/lib -lpthread -lsqlite3

all:
	$(CC) main.c register.c security.c -o $(BIN) $(REGISTER_FLAGS) 

clean:
	rm -rf $(BIN)
