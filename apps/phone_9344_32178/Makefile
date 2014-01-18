BIN=phone_control
OBJ=phone_main.o phone_control.o phone_audio.o phone_action.o dtmf_all_raw_data_100ms_big.o fsk_parse.o

PHONE_CFLAGS += -I ../include
PHONE_LD_FLAGS += -lpthread -lm

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip

export BIN
export OBJ
export PHONE_CFLAGS
export PHONE_LD_FLAGS
export CC
export STRIP

all:
		cd src;$(MAKE) clean;$(MAKE)
clean:
		cd src;$(MAKE) clean;
