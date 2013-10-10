
ifeq ($(ATH_GMAC_AR934x),1)

MAC-OBJS += ag934x.o

ifeq ($(ETH_CONFIG),_s16)
PHY-OBJS += phys/athrs16_phy.o
ATH_GMAC_GE1_IS_CONNECTED=0
ATH_GMAC0_RGMII=1
endif

ifeq ($(ETH_CONFIG),_s17)
PHY-OBJS += phys/athrs17_phy.o phys/ssdk_ks_km.a
ATH_GMAC0_RGMII=1
ATH_GMAC_GE1_IS_CONNECTED=0
PRJ_PATH=/home/jhung/WASP/9.2.999.28/ssdk/s17_wasp/board/ssdk_header

EXTRA_CFLAGS += -I$(PRJ_PATH)/include \
          -I$(PRJ_PATH)/include/common \
          -I$(PRJ_PATH)/include/fal \
          -I$(PRJ_PATH)/include/hsl \
          -I$(PRJ_PATH)/include/hsl/isis \
          -I$(PRJ_PATH)/include/sal/os \
          -I$(PRJ_PATH)/include/sal/sd \
          -I$(PRJ_PATH)/include/sal/sd/linux/hydra_howl \
          -I$(PRJ_PATH)/include/sal/sd/linux/uk_interface \
          -I$(PRJ_PATH)/include/init

endif

ifeq ($(ETH_CONFIG),_f1e)
PHY-OBJS += phys/athrf1_phy.o
ATH_GMAC_GE1_IS_CONNECTED=0
ATH_GMAC0_RGMII=1
endif

ifeq ($(ETH_CONFIG),_vir)
PHY-OBJS += phys/athrs_vir_phy.o
ATH_GMAC_HW_QOS=0 
ATH_GMAC_GE1_IS_CONNECTED=0
ATH_GMAC0_RGMII=1
endif

ifeq ($(ETH_CONFIG),_s27)
PHY-OBJS += phys/athrs27_phy.o
ATH_GMAC0_MII=1
ATH_GMAC1_RGMII=1
endif 

ifeq ($(ATH_GMAC_SUPPORT_DUAL_PHY),1)
ATH_GMAC_GE1_IS_CONNECTED=1
ATH_GMAC0_GMII=0
ATH_GMAC0_MII=0

ifeq ($(ETH_CONFIG2),_s16)
PHY-OBJS += phys/athrs16_phy.o
ATH_GMAC0_RGMII=1
endif

ifeq ($(ETH_CONFIG2),_s17)
PHY-OBJS += phys/athrs17_phy.o
ATH_GMAC0_RGMII=1
endif

ifeq ($(ETH_CONFIG2),_f1e)
PHY-OBJS += phys/athrf1_phy.o
ATH_GMAC0_RGMII=1
endif

endif # ATH_GMAC_SUPPORT_DUAL_PHY
endif # ATH_GMAC_AR934x

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
ifeq ($(ATH_GMAC_AR934x),1)

	@> include/config.h
	@echo "/*" > include/config.h
	@echo "* Automatically generated make config: don't edit" >> include/config.h
	@echo "*/" >> include/config.h

ifeq ($(ATH_GMAC_SUPPORT_DUAL_PHY),1)
	@ echo "#define CONFIG_ATHR_SUPPORT_DUAL_PHY 1" >>include/config.h
ifeq ($(ETH_CONFIG2),_s16)
	@echo "#define CONFIG_ATHRS16_PHY 1" >>include/config.h
endif
ifeq ($(ETH_CONFIG2),_s17)
	@echo "#define CONFIG_ATHRS17_PHY 1" >>include/config.h
endif
ifeq ($(ETH_CONFIG2),_f1e)
	@echo "#define CONFIG_ATHRF1_PHY 1" >>include/config.h
endif
endif # ATH_GMAC_SUPPORT_DUAL_PHY

ifeq ($(ETH_CONFIG),_s16)
	@echo "#define CONFIG_ATHRS16_PHY 1" >>include/config.h
endif
ifeq ($(ETH_CONFIG),_s17)
	@echo "#define CONFIG_ATHRS17_PHY 1" >>include/config.h
endif
ifeq ($(ETH_CONFIG),_f1e)
	@echo "#define CONFIG_ATHRF1_PHY 1" >>include/config.h
endif
ifeq ($(ETH_CONFIG),_vir)
	@echo "#define CONFIG_ATHRS_VIR_PHY 1" >>include/config.h
endif
ifeq ($(ETH_CONFIG),_s27)
	@echo "#define CONFIG_ATHRS27_PHY 1" >>include/config.h
endif 

endif # ATH_GMAC_AR934x

ifeq ($(ATH_GMAC_HW_NAT),1)
	@echo "#define CONFIG_ATHRS_HW_NAT 1" >>include/config.h
endif

ifeq ($(ATH_GMAC_HW_ACL),1)
	@echo "#define CONFIG_ATHRS_HW_ACL 1" >>include/config.h
endif

ifeq ($(ATH_GMAC_HW_QOS),1)
	@echo "#define CONFIG_ATHRS_QOS 1" >>include/config.h
endif

ifeq ($(ATH_GMAC_RX_TASKLET),1)
	@echo "#define CONFIG_ATHR_RX_TASK 1" >>include/config.h
	@echo "#define CONFIG_ATHR_RX_PKTS_CNT $(ATH_GMAC_RX_PKT_CNT)" >>include/config.h
endif

ifeq ($(ATH_GMAC0_RXFCTL),1)
	@echo "#define CONFIG_GMAC0_RXFCTL 1" >>include/config.h
endif

ifeq ($(ATH_GMAC0_TXFCTL),1)
	@echo "#define CONFIG_GMAC0_TXFCTL 1" >>include/config.h
endif

ifeq ($(ATH_GMAC1_RXFCTL),1)
	@echo "#define CONFIG_GMAC1_RXFCTL 1" >>include/config.h
endif

ifeq ($(ATH_GMAC1_TXFCTL),1)
	@echo "#define CONFIG_GMAC1_TXFCTL 1" >>include/config.h
endif
ifeq ($(ATH_GMAC_GE0_IS_CONNECTED),1)
	@echo "#define CONFIG_AG7240_GE0_IS_CONNECTED 1" >> include/config.h
endif
ifeq ($(ATH_GMAC_GE1_IS_CONNECTED),1)
	@echo "#define CONFIG_AG7240_GE1_IS_CONNECTED 1" >> include/config.h
endif

ifeq ($(ATH_GMAC0_RGMII),1)
	@echo "#define CONFIG_AG7240_GE0_RGMII 1" >> include/config.h
endif
ifeq ($(ATH_GMAC0_GMII),1)
	@echo "#define CONFIG_AG7240_GE0_GMII 1" >> include/config.h
endif
ifeq ($(ATH_GMAC0_MII),1)
	@echo "#define CONFIG_AG7240_GE0_MII 1" >> include/config.h
endif

ifeq ($(ATH_GMAC1_RGMII),1)
	@echo "#define CONFIG_AG7240_GE1_RGMII 1" >> include/config.h
endif
ifeq ($(ATH_GMAC1_GMII),1)
	@echo "#define CONFIG_AG7240_GE1_GMII 1" >> include/config.h
endif
ifeq ($(ATH_GMAC1_MII),1)
	@echo "#define CONFIG_AG7240_GE1_MII 1" >> include/config.h
endif
ifeq ($(ATH_SWITCH_ONLY_MODE),1)
	@echo "#define CONFIG_ATHR_SWITCH_ONLY_MODE 1" >> include/config.h
endif

	@echo "#define CONFIG_AG7240_LEN_PER_TX_DS $(ATH_GMAC_LEN_PER_TX_DS)" >> include/config.h
	@echo "#define CONFIG_AG7240_NUMBER_TX_PKTS  $(ATH_GMAC0_TX_CNT)" >>include/config.h
	@echo "#define CONFIG_AG7240_NUMBER_RX_PKTS   $(ATH_GMAC0_RX_CNT)" >>include/config.h
	@echo "#define CONFIG_AG7240_NUMBER_TX_PKTS_1 $(ATH_GMAC1_TX_CNT)" >>include/config.h
	@echo "#define CONFIG_AG7240_NUMBER_RX_PKTS_1 $(ATH_GMAC1_RX_CNT)" >>include/config.h
	@echo "#define CONFIG_AG7240_MAC_LOCATION $(ATH_GMAC_LOCATION)" >>include/config.h

