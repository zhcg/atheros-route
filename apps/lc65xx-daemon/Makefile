BIN = lc65xx-daemon
OBJ=lc65xx-daemon.o lc65xx-interface.o lc65xx-uart.o serial.o

LC65XX_CFLAGS +=  -D$(BUILD_VERSION) -I ../include -I ../../terminal_dev_register/sqlite-mips-linux/include
LC65XX_LD_FLAGS += -L ../../terminal_dev_register/sqlite-mips-linux/lib -L ../../libupdate -lupdate -lmjson -lpthread -lm -lsqlite3

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip
#CC = mips-linux-uclibc-gcc
export BIN
export OBJ
export LC65XX_CFLAGS
export LC65XX_LD_FLAGS
export CC
export STRIP

all:
		@echo VERSION:$(BUILD_VERSION)
		cd src;$(MAKE) clean;$(MAKE)
clean:
		cd src;$(MAKE) clean;
