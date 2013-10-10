export TOPDIR=$(PWD)/..
#export TOOLPATH=$(TOPDIR)/../../build/gcc-3.4.4-2.16.1/build_mips_nofpu
export TOOLPATH=$(TOPDIR)/../../build/gcc-4.3.3/build_mips/staging_dir/usr/

export UTILITYPATH=$(TOPDIR)/../utility/bin
export PATH:=$(TOPDIR)/tools:$(TOOLPATH)/bin:$(TOPDIR)/src:${PATH}

ifndef ATH_FW_ADDR
ATH_FW_ADDR = 0xa0001100
endif

CHIP=wasp
LOADADDR = 0xa1000000
LDFLAGS = -N -T../$(CHIP)_ramboot.lds
OPFILE =-burn

CROSS_COMPILE = mips-linux-uclibc-

OBJS =	start.o \
	burn.o \
	../nand_flash.o \
	../clock.o \
	../time.o \
	../mem.o \
	../ar7240_serial.o \
	../printf.o

######## END OF CONFIGURATION #########


ifeq ($(V),)
silent = @
else
silent = ""
endif

.S.s:
	@echo $(abspath $(TOOLPATH)/bin/$(CPP)) $(AFLAGS) $< -o $*.s > .$(notdir $*).s.cmd
	@echo '[CPP] ' $<
	$(silent)$(CPP) $(AFLAGS) $< -o $(notdir $*).s

.S.o:
	@echo $(abspath $(TOOLPATH)/bin/$(CC)) $(AFLAGS) -c $< -o $(notdir $*).o > .$(notdir $*).o.cmd
	@echo '[CC] ' $<
	$(silent)$(CC) $(AFLAGS) -c $< -o $(notdir $*).o

.c.o:
	@echo $(abspath $(TOOLPATH)/bin/$(CC)) $(CFLAGS) -c $< -o $(notdir $*).o > .$(notdir $*).o.cmd
	@echo '[CC] ' $<
	$(silent)$(CC) $(CFLAGS) -c $< -o $(notdir $*).o

CC =       $(CROSS_COMPILE)gcc
LD =       $(CROSS_COMPILE)ld
NM =       $(CROSS_COMPILE)nm
OBJCOPY =  $(CROSS_COMPILE)objcopy
OBJDUMP =  $(CROSS_COMPILE)objdump
BIN2HEX =  $(UTILITYPATH)/bin2hex

AFLAGS = -Werror -Wa,-gstabs -D__ASSEMBLY__ -g  -Os $(BUILD_DFLAGS) -D__KERNEL__ -I$(TOPDIR) -I$(TOPDIR)/include -fno-builtin -ffreestanding -nostdinc -isystem $(TOPDIR)/../../build/gcc-3.4.4-2.16.1/build_mips_nofpu/bin-ccache/../lib/gcc/mips-linux-uclibc/3.4.4/include -pipe  -DCONFIG_MIPS -D__MIPS__  -mabicalls -march=mips32r2 -mno-abicalls -fno-pic
CFLAGS = -Werror -Wall -Wa,-gstabs -g  -Os $(BUILD_DFLAGS) -DCONFIG_LZMA -D__KERNEL__ -I$(TOPDIR) -I$(TOPDIR)/include -fno-builtin -ffreestanding -nostdinc -isystem $(TOPDIR)/../../build/gcc-3.4.4-2.16.1/build_mips_nofpu/bin-ccache/../lib/gcc/mips-linux-uclibc/3.4.4/include -pipe  -DCONFIG_MIPS -D__MIPS__  -mabicalls -march=mips32r2 -mno-abicalls -fno-pic

CFLAGS += -DATH_FW_ADDR=$(ATH_FW_ADDR)u

# Drop some uninteresting sections in the kernel.
# This is only relevant for ELF kernels but doesn't hurt a.out
drop-sections   = .reginfo .mdebug
strip-flags     = $(addprefix --remove-section=,$(drop-sections))

.PHONY : bin

all : clean elf bin

elf : $(CHIP)$(OPFILE).elf
bin : $(CHIP)$(OPFILE).bin

$(CHIP)$(OPFILE).elf : $(OBJS)
	@echo Linking $(CHIP)$(OPFILE).elf ...
	@echo $(abspath $(TOOLPATH)/bin/$(LD)) -o $(CHIP)$(OPFILE).elf $(LDFLAGS) -Ttext $(LOADADDR) $(notdir $^) > .$(CHIP)$(OPFILE).elf.cmd
	$(silent)$(LD) -o $(CHIP)$(OPFILE).elf $(LDFLAGS) -Ttext $(LOADADDR) $(notdir $^)

$(CHIP)$(OPFILE).bin : $(CHIP)$(OPFILE).elf
	@echo Generating $(CHIP)$(OPFILE).bin ...
	@echo $(abspath $(TOOLPATH)/bin/$(OBJCOPY)) -S -O binary $(strip-flags) $(CHIP)$(OPFILE).elf $(CHIP)$(OPFILE).bin > .$(CHIP)$(OPFILE).bin.cmd
	$(silent)$(OBJCOPY) -S -O binary $(strip-flags) $(CHIP)$(OPFILE).elf $(CHIP)$(OPFILE).bin
ifdef TFTPPATH
	cp $(CHIP)$(OPFILE).bin $(TFTPPATH)
endif
	@echo ______instructions________
	@echo re
	@echo re
	@echo load $(ATH_FW_ADDR) 2fw.bin
	@echo load $(LOADADDR) $(TFTPPATH)$(CHIP)$(OPFILE).bin bin
	@echo go

clean:
	$(silent)rm -f *~ $(OBJS) *.bin *.map *.hex $(CHIP)$(OPFILE).elf
