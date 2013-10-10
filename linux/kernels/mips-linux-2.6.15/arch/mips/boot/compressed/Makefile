#include $(TOPDIR)/rules.mk

#KDIR:=$(BUILD_DIR)/../linux
KDIR:=../../../..
STAGING_DIR = $(TOPDIR)/build/util

include $(KDIR)/.config
CROSS_COMPILE = mips-linux-uclibc-
OBJDUMP = $(CROSS_COMPILE)objdump
OBJCOPY = $(CROSS_COMPILE)objcopy
LD = $(CROSS_COMPILE)ld
CC = $(CROSS_COMPILE)cc
NM = $(CROSS_COMPILE)nm

O_FORMAT = $(shell $(OBJDUMP) -i | head -2 | grep elf32)
OBJDUMP_VERSION := $(shell if $(OBJDUMP) -i | head -2 2>&1 | grep 'elf32-tradbigmips' >/dev/null; then echo y;else echo n;fi)
export OBJDUMP_VERSION

HPATH = $(KDIR)/include
CPPFLAGS := -D__KERNEL__ -I$(HPATH)
AFLAGS := -D__ASSEMBLY__ $(CPPFLAGS) 

CFLAGS := $(CPPFLAGS) -Wall -Wstrict-prototypes -Wno-trigraphs -Os \
          -fno-strict-aliasing -fno-common
CFLAGS += -fno-builtin-sprintf

ifndef CONFIG_FRAME_POINTER
  CFLAGS += -fomit-frame-pointer
endif

# disable pointer signedness warnings in gcc 4.0
CFLAGS += $(call check_gcc,-Wno-pointer-sign,)
CFLAGS += $(call check_gcc, -funit-at-a-time,)

GCCFLAGS        := -I $(KDDIR)/include/asm/gcc
GCCFLAGS        += -G 0 -mno-abicalls -fno-pic -pipe
GCCFLAGS        += $(call check_gcc, -finline-limit=100000,)
LINKFLAGS       += -G 0 -static -nostdlib
MODFLAGS        += -mlong-calls -fno-common

ifdef CONFIG_REMOTE_DEBUG
GCCFLAGS        += -g
endif

GCCFLAGS       	+= -march=mips32r2 -Wa,-32 -Wa,-march=mips32r2 -Wa,-mips32r2 -Wa,--trap

AFLAGS          += $(GCCFLAGS)
CFLAGS          += $(GCCFLAGS)

ZLDFLAGS = -static -e zstartup -T $(KDIR)/arch/mips/kernel/vmlinux.lds
ZLINKFLAGS = --defsym jiffies_64=0 -Ttext $(ZIMAGE_OFFSET) $(ZLDFLAGS)


HEAD = head.o
SYSTEM = $(KDIR)/vmlinux


all: $(TOPDIR)/zImage 

#all: 

ZIMAGE_OFFSET := 0x80800000

LOADADDR      := 0x$(shell $(NM) $(SYSTEM) | grep \ _text |cut -f1 -d' ')
KERNEL_ENTRY  := $(shell $(OBJDUMP) -f $(SYSTEM) | sed -n -e 's/^start address //p')


OBJECTS = $(HEAD) misc-lzma.o


dep:

install:

$(TOPDIR)/zImage: piggy.o $(OBJECTS)
	$(LD) $(ZLINKFLAGS) -o $(KDIR)/zImage.elf $(OBJECTS) piggy.o
#	@cp $(KDIR)/zImage.elf $(KDIR)/zImage.elf.backup 
	$(OBJCOPY) -S -O binary -R .note -R .comment $(KDIR)/zImage.elf zImage.lzma
	@rm -f *.o $(KDIR)/zImage.elf

head.o: head.S Makefile $(SYSTEM)
	$(CC) $(AFLAGS) -DKERNEL_ENTRY=$(KERNEL_ENTRY) -c head.S

misc-lzma.o: misc-lzma.c Makefile
	$(CC) $(CFLAGS) -DLOADADDR=$(LOADADDR) -c misc-lzma.c

piggy.o: $(SYSTEM)
#	@cp $(SYSTEM).bin $(SYSTEM).bin.lzma
	@$(OBJCOPY) -S -O binary -R .note -R .comment $(SYSTEM) $(SYSTEM).bin.lzma
	cat $(SYSTEM).bin.lzma | $(STAGING_DIR)/lzma e -d22 -si -so > $(SYSTEM).compress
	@echo "OUTPUT_ARCH(mips) SECTIONS { .initrd : { input_len = .; LONG(input_data_end - input_data) input_data = .; *(.data) input_data_end = .; }}" > ld.script
#	@echo "O_FORMAT:  " $(O_FORMAT); 
	$(LD) -T ld.script -b binary --oformat $(O_FORMAT) -o piggy.o $(SYSTEM).compress
	rm -f $(SYSTEM).bin.lzma ld.script $(SYSTEM).compress


clean:
	rm -f *.o $(KDIR)/zImage.lzma
mrproper:
	rm -f *.o $(KDIR)/zImage.lzma


