
ifeq ($(ATH_GMAC_AR7100),1)

MAC-OBJS += ag7100_mod.o

ifeq ($(ETH_CONFIG),_s16)
PHY-OBJS += phys/athrs16_phy.o
ATH_GMAC_HW_QOS=0 
ATH_GMAC_GE1_IS_CONNECTED=0
ATH_GMAC0_RGMII=1
else

ifeq ($(ETH_CONFIG),_f1e)
PHY-OBJS += athrmagpie_phy.o
PHY-OBJS += athrf1_phy.o
ATH_GMAC_HW_QOS=0 
AG7100_GE1_IS_CONNECTED=1
AG7100_GE0_RGMII=1
AG7100_GE1_RGMII=1
PORT0_AS_SWITCH=0
else

ifeq ($(ETH_CONFIG),_vir)
PHY-OBJS += phys/athrs_vir_phy.o
ATH_GMAC_HW_QOS=0 
ATH_GMAC_GE1_IS_CONNECTED=0
ATH_GMAC0_RGMII=1
else


PHY-OBJS += phys/athrs27_phy.o
ATH_GMAC0_MII=1
ATH_GMAC1_RGMII=1
endif # _vir
endif # _fle
endif # _s16

endif # ATH_GMAC_AR7100

ifeq ($(ATH_GMAC_HW_NAT),1)
ACCEL-OBJS += athrs_nf_nat.o athrs_hw_nat.o
endif

ifeq ($(ATH_GMAC_HW_ACL),1)
ACCEL-OBJS += athrs_nf_acl.o athrs_hw_acl.o
endif

ifeq ($(ATH_GMAC_HW_QOS),1)
ACCEL-OBJS += athrs_qos.o vlan_igmp.o
endif


enet_config.h:
ifeq ($(ATH_GMAC_AR7100),1)

	@> config.h
	@echo "/*" > config.h
	@echo "* Automatically generated make config: don't edit" >> config.h
	@echo "*/" >> config.h

ifeq ($(ETH_CONFIG),_s16)
	@echo "#define CONFIG_ATHRS16_PHY 1" >>config.h
else
ifeq ($(ETH_CONFIG),_f1e)
	@echo "#define CONFIG_ATHRF1_PHY 1" >>config.h
else
ifeq ($(ETH_CONFIG),_vir)
	@echo "#define CONFIG_ATHRS_VIR_PHY 1" >>config.h
else
	@echo "#define CONFIG_ATHRS27_PHY 1" >>config.h
endif # _vir
endif # _fle
endif # _s16

endif # ATH_GMAC_AR7100

ifeq ($(AG7100_GE1_IS_CONNECTED),1)
	@echo "#define CONFIG_AG7100_GE0_IS_CONNECTED 1" >> config.h
endif
ifeq ($(AG7100_GE1_IS_CONNECTED),1)
	@echo "#define CONFIG_AG7100_GE1_IS_CONNECTED 1" >> config.h
endif

ifeq ($(AG7100_GE0_RGMII),1)
	@echo "#define CONFIG_AG7100_GE0_RGMII 1" >> config.h
endif
ifeq ($(AG7100_GE1_RGMII),1)
	@echo "#define CONFIG_AG7100_GE1_RGMII 1" >> config.h
endif
ifeq ($(PORT0_AS_SWITCH),1)
	@echo "#define CONFIG_PORT0_AS_SWITCH 1" >> config.h
endif

	@echo "#define CONFIG_AG7100_LEN_PER_TX_DS $(AG7100_LEN_PER_TX_DS)" >> config.h
	@echo "#define CONFIG_AG7100_NUMBER_TX_PKTS  $(AG7100_TX_PKTS)" >>config.h
	@echo "#define CONFIG_AG7100_NUMBER_RX_PKTS  $(AG7100_RX_PKTS)" >>config.h
	@echo "#define CONFIG_AG7100_MAC_LOCATION $(AG7100_MAC_LOCATION)" >>config.h

