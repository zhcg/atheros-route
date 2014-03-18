BIN=OTA_Base
OBJ=versions_up_base.o  our_md5.o
export PATH = ../../build/gcc-4.3.3/build_mips/staging_dir/usr/bin:$PATH
export TOOLPREFIX=mips-linux-uclibc-
CROSS:=$(TOOLPREFIX)
CC=$(CROSS)gcc
STRIP           =$(CROSS)strip


all:$(BIN)

$(BIN):$(OBJ)
		$(CC) -o $(BIN) $^ -lpthread

.c.o:
		$(CC) -c $^

clean:
		rm -rf $(BIN) $(OBJ)
