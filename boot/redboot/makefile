#
# Take care of some paths
#

ifndef ATH_ROOT
ATH_ROOT=$(PWD)/../..
endif

ifndef ATH_INSTALL
ATH_INSTALL=$(PWD)
endif

ifndef TFTPPATH
TFTPPATH=/tftpboot/`whoami`
endif

ifndef BOARD_TYPE
BOARD_TYPE=tb225
endif


export ECOS_REPOSITORY=$(PWD)/ecos/packages
PATH:=${PATH}:$(ATH_ROOT)/build/mipsisa32-elf/bin:$(PWD)/ecos/tools/bin

ROMBUILD_DIR=$(ATH_INSTALL)/rom_bld
RAMBUILD_DIR=$(ATH_INSTALL)/ram_bld

#
# Rule helpers
#

PLATFORM_ROM_START = rm -rf $(ROMBUILD_DIR); \
	mkdir -p $(ROMBUILD_DIR); \
	cd $(ROMBUILD_DIR); \
	ecosconfig new $(BOARD_TYPE) redboot;

PLATFORM_RAM_START = rm -rf $(RAMBUILD_DIR); \
	mkdir -p $(RAMBUILD_DIR); \
	cd $(RAMBUILD_DIR); \
	ecosconfig new $(BOARD_TYPE) redboot;

PLATFORM_ROM_BUILD = make -C $(ROMBUILD_DIR); \
	cd $(ROMBUILD_DIR)/install/bin; \
	mipsisa32-elf-objcopy -O binary redboot.elf redboot.rom ;\
	cp redboot.rom $(TFTPPATH)/redboot.rom

PLATFORM_RAM_BUILD = make -C $(RAMBUILD_DIR); \
	cd $(RAMBUILD_DIR)/install/bin; \
	mipsisa32-elf-objcopy -O binary redboot.elf redboot.bin; \
	cp redboot.bin $(TFTPPATH)/redboot.ram


#
# The rules
#


all: tb225_ram tb225_rom pb42_ram pb42_rom pb44_ram pb44_rom pb45_ram pb45_rom


tb225_ram:	BOARD_TYPE=tb225
tb225_ram:
# RAM version
	$(PLATFORM_RAM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/tb225/current/misc/redboot_RAM.ecm; \
    ecosconfig import $(ATH_INSTALL)/options/eth/vsc_phy; \
	ecosconfig tree; \
	$(PLATFORM_RAM_BUILD)


tb225_rom:	BOARD_TYPE=tb225
tb225_rom:
# ROMRAM version
	$(PLATFORM_ROM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/tb225/current/misc/redboot_ROMRAM.ecm; \
    ecosconfig import $(ATH_INSTALL)/options/eth/vsc_phy; \
	ecosconfig tree; \
	$(PLATFORM_ROM_BUILD)


pb42_ram:	BOARD_TYPE=pb42
pb42_ram:
# RAM version
	$(PLATFORM_RAM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/pb42/current/misc/redboot_RAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/adm_phy; \
	ecosconfig tree; \
	$(PLATFORM_RAM_BUILD)


pb42_rom:	BOARD_TYPE=pb42
pb42_rom:
# ROMRAM version
	$(PLATFORM_ROM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/pb42/current/misc/redboot_ROMRAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/adm_phy; \
	ecosconfig tree; \
	$(PLATFORM_ROM_BUILD)


ap94_ram:	BOARD_TYPE=ap94
ap94_ram:
# RAM version
	$(PLATFORM_RAM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/ap94/current/misc/redboot_RAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/athrs26_phy; \
	ecosconfig tree; \
	$(PLATFORM_RAM_BUILD)


ap94_rom:	BOARD_TYPE=ap94
ap94_rom:
# ROMRAM version
	$(PLATFORM_ROM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/ap94/current/misc/redboot_ROMRAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/athrs26_phy; \
	ecosconfig tree; \
	$(PLATFORM_ROM_BUILD)

pb44_ram:       BOARD_TYPE=pb44
pb44_ram:
# RAM version
	$(PLATFORM_RAM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/pb44/current/misc/redboot_RAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/vsc8601_phy; \
	ecosconfig tree; \
	$(PLATFORM_RAM_BUILD)


pb44_rom:       BOARD_TYPE=pb44
pb44_rom:
# ROMRAM version
	$(PLATFORM_ROM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/pb44/current/misc/redboot_ROMRAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/vsc8601_phy; \
	ecosconfig tree; \
	$(PLATFORM_ROM_BUILD)

pb45_ram:       BOARD_TYPE=pb45
pb45_ram:
# RAM version
	$(PLATFORM_RAM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/pb45/current/misc/redboot_RAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/athr_phy; \
	ecosconfig tree; \
	$(PLATFORM_RAM_BUILD)


pb45_rom:       BOARD_TYPE=pb45
pb45_rom:
# ROMRAM version
	$(PLATFORM_ROM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/pb45/current/misc/redboot_ROMRAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/athr_phy; \
	ecosconfig tree; \
	$(PLATFORM_ROM_BUILD)

cus100_ram:       BOARD_TYPE=cus100
cus100_ram:
# RAM version
	$(PLATFORM_RAM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/cus100/current/misc/redboot_RAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/athrs26_phy; \
	ecosconfig tree; \
	$(PLATFORM_RAM_BUILD)


cus100_rom:       BOARD_TYPE=cus100
cus100_rom:
# ROMRAM version
	$(PLATFORM_ROM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/cus100/current/misc/redboot_ROMRAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/athrs26_phy; \
	ecosconfig tree; \
	$(PLATFORM_ROM_BUILD)

pb44-router_ram:       BOARD_TYPE=pb44-router
pb44-router_ram:
# RAM version
	$(PLATFORM_RAM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/pb44-router/current/misc/redboot_RAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/vsc8601_phy; \
	ecosconfig tree; \
	$(PLATFORM_RAM_BUILD)


pb44-router_rom:       BOARD_TYPE=pb44-router
pb44-router_rom:
# ROMRAM version
	$(PLATFORM_ROM_START) \
	ecosconfig import $(ECOS_REPOSITORY)/hal/mips/pb44-router/current/misc/redboot_ROMRAM.ecm; \
	ecosconfig import $(ATH_INSTALL)/options/eth/vsc8601_phy; \
	ecosconfig tree; \
	$(PLATFORM_ROM_BUILD)


