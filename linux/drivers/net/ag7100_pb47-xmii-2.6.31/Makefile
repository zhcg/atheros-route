#
# Makefile for Ar7100 gigebit MAC and Phys
#
#
# Makefile for Ar7100 gigebit MAC and Phys
#
include ${ENETDIR}/Makefile.inc 

ifeq ($(ATH_GMAC_AR7100),1)
obj-m			+=	ag7100_mod.o
ag7100_mod-objs	:=  $(ACCEL-OBJS) $(PHY-OBJS) $(MAC-OBJS) ag7100.o

else

obj-$(CONFIG_AG7100)           += ag7100_mod.o
obj-phy-$(CONFIG_VITESSE_PHY)	=	vsc_phy.o
obj-phy-$(CONFIG_ICPLUS_PHY)	=	ipPhy.o
obj-phy-$(CONFIG_REALTEK_PHY)	=	rtPhy.o
obj-phy-$(CONFIG_ADM6996FC_PHY)	=	adm_phy.o
ifeq ($(CONFIG_AP81_CARRIER_CUS109), 1)
obj-phy-$(CONFIG_ATHRF1_PHY) 	=	athrf1_phy.o
else
#ifeq ($(CONFIG_MACH_AR7100_PB47),y)
CONFIG_MAGPIE_PHY=y
obj-phy-$(CONFIG_MAGPIE_PHY) += athrmagpie_phy.o
obj-phy-$(CONFIG_ATHRF1_PHY) 	+=	athrf1_phy.o
#else
#obj-phy-$(CONFIG_ATHR_PHY) 	=	athrf1_phy.o
#endif 
endif 
#obj-phy-$(CONFIG_ATHRS26_PHY) 	=	athrs26_phy.o
#obj-phy-$(CONFIG_ATHRS16_PHY) 	=	athrs16_phy.o ssdk_mod.a
#obj-phy-$(CONFIG_AG7100_GENERIC_PHY)    = generic_phy.o
#obj-phy-$(CONFIG_VITESSE_8601_PHY)      = vsc8601_phy.o
#obj-phy-$(CONFIG_VITESSE_8601_7395_PHY) = vsc8601_phy.o generic_i2c.o generic_spi.o vsc73xx.o


ag7100_mod-objs			:=	$(obj-phy-y) ag7100.o

endif
EXTRA_CFLAGS += -I ${ENETDIR}/include -I ${ENETDIR}/include/phys -I$(KERNELPATH)/arch/mips/include

ifeq ($(CONFIG_AP81_CARRIER_CUS109), 1)
# There are two F1e PHY on the CUS109 board.
# One PHY connect to MAC 0, the other one connects to MAC 1.
# Define this symbol to indicate two PHY connect to AR913X.
CFLAGS_athrf1_phy.o += -DCONFIG_CUS109_F1E_PHY
CFLAGS_ag7100.o += -DCONFIG_DUAL_F1E_PHY
endif

clean:
	rm -f *.o *.ko 
	rm -f phys/*.o *.ko
