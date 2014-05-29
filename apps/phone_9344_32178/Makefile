BIN = phone_control
OBJ=phone_main.o phone_control.o phone_audio.o phone_action.o dtmf_all_raw_data_100ms.o security.o fsk_parse.o mdf.o preprocess.o fftwrap.o filterbank.o smallft.o kiss_fftr.o kiss_fft.o

PHONE_CFLAGS +=  -D$(BUILD_VERSION) -I ../include -I ../../terminal_dev_register/sqlite-mips-linux/include
PHONE_LD_FLAGS += -L ../../terminal_dev_register/sqlite-mips-linux/lib -lpthread -lm -lsqlite3

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip
#CC = mips-linux-uclibc-gcc
export BIN
export OBJ
export PHONE_CFLAGS
export PHONE_LD_FLAGS
export CC
export STRIP

all:
		@echo VERSION:$(BUILD_VERSION)
		cd src;$(MAKE) clean;$(MAKE)
clean:
		cd src;$(MAKE) clean;
