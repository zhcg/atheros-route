export TOPDIR=$(PWD)/
#export TOOLPATH=$(TOPDIR)/../../build/gcc-3.4.4-2.16.1/build_mips_nofpu
export TOOLPATH=$(TOPDIR)/../../build/gcc-4.3.3/build_mips/staging_dir/usr/
export KERNELPATH=$(TOPDIR)/../../linux/kernels/mips-linux-2.6.15
export UTILITYPATH=$(TOPDIR)/../utility/bin
export PATH:=$(TOPDIR)/tools:$(TOOLPATH)/bin:$(TOPDIR)/src:${PATH}

#
# Target Specific Variable values
# http://www.gnu.org/software/autoconf/manual/make/Target_002dspecific.html
#

#
# Config for RAM
#
fpga : BUILD_DFLAGS =
fpga : BUILD_BIN=rom_fpga.bin
fpga : LOADADDR = 0xa1000000
fpga : AFLAGS += -DRAM_BUILD
fpga : CFLAGS += -DRAM_BUILD
fpga : LDFLAGS = -N -T$(CHIP)_ramboot.lds
fpga : OPFILE =_ramboot

#
# config for ROM
#
asic : BUILD_DFLAGS = "-D_ROM_"
asic : BUILD_BIN=rom_asic.bin
asic : LOADADDR = 0xbfc00000
asic : AFLAGS += -DROM_BUILD
asic : CFLAGS += -DROM_BUILD -DCONFIG_WASP_EMU_NO_OTP=0
asic : LDFLAGS = -N -T$(CHIP)_romboot.lds
asic : OPFILE =_romboot

#
# config for flash
#
flash : BUILD_DFLAGS = "-D_ROM_"
flash : BUILD_BIN=rom_flash.bin
flash : LOADADDR = 0x9f000000
flash : AFLAGS += -DROM_BUILD
flash : CFLAGS += -DROM_BUILD -DCONFIG_WASP_EMU_NO_OTP=0
flash : LDFLAGS = -N -T$(CHIP)_flashboot.lds
flash : OPFILE =_flashboot

#
# flash-build run on ram
#
flram : BUILD_DFLAGS = "-D_ROM_"
flram : BUILD_BIN=rom_asic.bin
flram : LOADADDR = 0xa1000000
flram : AFLAGS += -DROM_BUILD
flram : CFLAGS += -DROM_BUILD -DCONFIG_WASP_EMU_NO_OTP=0
flram : LDFLAGS = -N -T$(CHIP)_flashboot.lds
flram : OPFILE =_flramboot

#
# config for Emulation ROM
#
erom : BUILD_DFLAGS =
erom : BUILD_BIN=rom_fpga.bin
erom : LOADADDR = 0xbfc00000
erom : AFLAGS += -DRAM_BUILD -DEMUROM_BUILD
erom : CFLAGS += -DRAM_BUILD -DEMUROM_BUILD
erom : LDFLAGS = -N -T$(CHIP)_romboot.lds
erom : OPFILE =_eromboot

ifeq ($(otp),1)
erom : CFLAGS += -DCONFIG_WASP_EMU_NO_OTP=0
fpga : CFLAGS += -DCONFIG_WASP_EMU_NO_OTP=0
else
erom : CFLAGS += -DCONFIG_WASP_EMU_NO_OTP=1
fpga : CFLAGS += -DCONFIG_WASP_EMU_NO_OTP=1
endif

CHIP=wasp

CROSS_COMPILE = mips-linux-uclibc-

OBJS =	start.o \
	cache.o \
	time.o \
	ar7240_serial.o \
	printf.o \
	lowlevel_init.o \
	main.o \
	mem.o \
	usb_dev.o \
	mdio_fwd.o \
	cpu.o \
	rompatch.o \
	otp.o \
	clock.o \
	exception.o \
	hif.o \
	nand_flash.o \
	hif_pci.o \
	dma_lib.o \
	dma_engine.o \
	cmnos_allocram.o \
	desc.o \
	vdesc.o \
	vbuf.o \
	fwd.o \
	gmac.o \
	phy.o \
	LzmaDecode.o LzmaWrapper.o

######## END OF CONFIGURATION #########

.S.s:
	$(CPP) $(AFLAGS) $< -o $*.s
.S.o:
	$(CC) $(AFLAGS) -c $< -o $*.o
.c.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

CC =       $(CROSS_COMPILE)gcc
LD =       $(CROSS_COMPILE)ld
NM =       $(CROSS_COMPILE)nm
OBJCOPY =  $(CROSS_COMPILE)objcopy
OBJDUMP =  $(CROSS_COMPILE)objdump
BIN2HEX =  $(UTILITYPATH)/bin2hex

AFLAGS = -Werror -Wa,-gstabs -D__ASSEMBLY__ -g  -Os $(BUILD_DFLAGS) -D__KERNEL__ -I$(TOPDIR) -I$(TOPDIR)/include -fno-builtin -ffreestanding -nostdinc -isystem $(TOPDIR)/../../build/gcc-3.4.4-2.16.1/build_mips_nofpu/bin-ccache/../lib/gcc/mips-linux-uclibc/3.4.4/include -pipe  -DCONFIG_MIPS -D__MIPS__  -mabicalls -march=mips32r2 -mno-abicalls -fno-pic
CFLAGS = -Werror -Wall -Wa,-gstabs -g  -Os $(BUILD_DFLAGS) -DCONFIG_LZMA -D__KERNEL__ -I$(TOPDIR) -I$(TOPDIR)/include -fno-builtin -ffreestanding -nostdinc -isystem $(TOPDIR)/../../build/gcc-3.4.4-2.16.1/build_mips_nofpu/bin-ccache/../lib/gcc/mips-linux-uclibc/3.4.4/include -pipe  -DCONFIG_MIPS -D__MIPS__  -mabicalls -march=mips32r2 -mno-abicalls -fno-pic


# Drop some uninteresting sections in the kernel.
# This is only relevant for ELF kernels but doesn't hurt a.out
drop-sections   = .reginfo .mdebug
strip-flags     = $(addprefix --remove-section=,$(drop-sections))

all :
	@echo ===========================
	@echo make '{ asic | fpga | erom | flash | flram }'
	@echo ===========================

.PHONY : bin

asic : clean elf srec fulldump bin
fpga : clean elf srec fulldump bin
erom : clean elf srec fulldump bin
flash : clean elf srec fulldump bin
flram : clean elf srec fulldump bin

srec : $(CHIP)$(OPFILE).srec
elf : $(CHIP)$(OPFILE).elf
#bin : rom.hex $(CHIP)$(OPFILE).bin
bin : $(BUILD_BIN) rom.hex $(CHIP)$(OPFILE).bin

$(CHIP)$(OPFILE).elf : $(OBJS)
	$(LD) -Map $(CHIP)$(OPFILE).map -o $(CHIP)$(OPFILE).elf $(LDFLAGS) -Ttext $(LOADADDR) $^
	rm -f ../rom-boot-drv/rom.addrs.ld
	-./make_ld.sh $(CHIP)$(OPFILE).elf rom.symbols > ../rom-boot-drv/rom.addrs.c

$(CHIP)$(OPFILE).srec : $(CHIP)$(OPFILE).elf
	$(OBJCOPY) -S -O srec $(strip-flags) $(CHIP)$(OPFILE).elf $(CHIP)$(OPFILE).srec

$(CHIP)$(OPFILE).bin : $(CHIP)$(OPFILE).elf
	$(OBJCOPY) -S -O binary $(strip-flags) $(CHIP)$(OPFILE).elf $(CHIP)$(OPFILE).bin

#$(BUILD_BIN) : $(CHIP)$(OPFILE).elf
rom.hex :
	@$(OBJCOPY)		\
		-R .data	\
		-R .mdebug	\
		-O binary $(CHIP)$(OPFILE).elf rodata.bin
	@$(OBJCOPY)		\
		-j .data	\
		-O binary $(CHIP)$(OPFILE).elf rwdata.bin
	cat rodata.bin rwdata.bin > $(BUILD_BIN)
	@rm rodata.bin rwdata.bin
	@$(BIN2HEX) $(BUILD_BIN) rom.hex rom

fulldump :
	$(OBJDUMP) -Dlxs $(CHIP)$(OPFILE).elf > $(CHIP)$(OPFILE).objdump
	$(NM) -n $(CHIP)$(OPFILE).elf > $(CHIP)$(OPFILE).nm

clean:
	rm -f *~ *.o *.map *.hex *.srec *.objdump *.nm

export PF=testpatch
export OTP_OFFSET=0xb8130000
export OTP_PATCH_OFFSET=0x30
export OTP_PATCH_LINK_ADDR=bd001400

$(PF): patchclean $(PF).o
	$(LD) -Ttext 0x$(OTP_PATCH_LINK_ADDR) -e patchmain -o $(PF) $(PF).o
	$(OBJCOPY)		\
		-j .text	\
		-O binary $(PF) $(PF).bin
	$(OBJDUMP) -d $(PF) > $(PF).dis
	$(TOPDIR)/../../build/util/lzma e $(PF).bin $(PF).lzma
	@./mkpatch -l $(OTP_PATCH_LINK_ADDR) && echo $(PF).pch created
	@echo re
	@echo mm 0xb8131008 0xc8
	@echo mm 0xb8131024 0x1
	@echo md 0xb813102c 1
	@echo mm 0xb8131000 0x010ad079
	@perl -e 'printf "mm 0xb8130010 0x0000%x00\n", $(OTP_PATCH_OFFSET)/2'
	@xxd -c 4 -g 4 $(PF).pch | \
		awk '{print "0x"$$1" 0x"$$2}' | \
		perl -pe 's/0x[0-9a-f]+:/sprintf ("mm 0x%x", hex ($$&) + $(OTP_OFFSET) + $(OTP_PATCH_OFFSET))/e'

patchclean:
	@rm -f $(PF)  $(PF).lzma  $(PF).o $(PF).bin
