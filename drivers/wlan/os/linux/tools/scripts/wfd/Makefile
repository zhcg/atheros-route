#
# Makefile for WFD Scripts.
#
# Note : For WFD automation, PING need to support "-i" option.
#        Please upgrade busybox or install iputils.
#        Then, locate PING at "/usr/bin".
#

ifeq ($(obj),)
obj := .
endif

DEPTH := ../../../../..

include $(obj)/${DEPTH}/os/linux/Makefile.inc

all: wfd_scripts

clean: wfd_scripts_clean

# WFD Auto-test Script
wfd_scripts: wfd_scripts_clean
	@echo copy WFD scripts...
	mkdir -p $(INSTALL_ROOT)/home/atheros/Atheros-P2P/scripts/
	cp $(DEPTH)/os/linux/tools/scripts/wfd/ath_scripts/WFA-* $(INSTALL_ROOT)/root
	cp $(DEPTH)/os/linux/tools/scripts/wfd/ath_scripts/shortgioff.sh $(INSTALL_ROOT)/root
	cp $(DEPTH)/os/linux/tools/scripts/wfd/ath_scripts/p2pload $(INSTALL_ROOT)/etc/ath/p2p/
	cp $(DEPTH)/os/linux/tools/scripts/wfd/ath_scripts/setupp2p $(INSTALL_ROOT)/etc/ath/p2p/
	cp $(DEPTH)/os/linux/tools/scripts/wfd/ath_scripts/p2pdev.start $(INSTALL_ROOT)/etc/ath/p2p/
	cp $(DEPTH)/os/linux/tools/scripts/wfd/ath_scripts/p2pdev.conf $(INSTALL_ROOT)/etc/ath/p2p/
	cp $(DEPTH)/os/linux/tools/scripts/wfd/ath_scripts/wpa-sta.conf $(INSTALL_ROOT)/etc/ath/
	cp $(DEPTH)/os/linux/tools/scripts/wfd/wfd_scripts/*.sh $(INSTALL_ROOT)/home/atheros/Atheros-P2P/scripts/
	cp $(DEPTH)/os/linux/tools/scripts/wfd/wfd_scripts/udhcpc-p2p.* $(INSTALL_ROOT)/etc/
	cp $(DEPTH)/os/linux/tools/scripts/wfd/wfd_scripts/udhcpd-p2p.* $(INSTALL_ROOT)/etc/
	cp $(DEPTH)/os/linux/tools/scripts/wfd/ath_scripts/start-* $(INSTALL_ROOT)/usr/bin/
	chmod a+x $(INSTALL_ROOT)/root/WFA-*.sh
	chmod a+x $(INSTALL_ROOT)/root/shortgioff.sh
	chmod a+x $(INSTALL_ROOT)/etc/ath/p2p/*
	chmod a+x $(INSTALL_ROOT)/home/atheros/Atheros-P2P/scripts/*
	chmod a+x $(INSTALL_ROOT)/etc/udhcpc-p2p.*
	chmod a+x $(INSTALL_ROOT)/etc/udhcpd-p2p.*
	chmod a+x $(INSTALL_ROOT)/usr/bin/start-*

wfd_scripts_clean:
	rm -rf $(INSTALL_ROOT)/root/WFA-*.sh
	rm -rf $(INSTALL_ROOT)/root/shortgioff.sh
	rm -rf $(INSTALL_ROOT)/etc/ath/p2p/p2pload
	rm -rf $(INSTALL_ROOT)/etc/ath/p2p/setupp2p
	rm -rf $(INSTALL_ROOT)/etc/ath/p2p/p2pdev.start
	rm -rf $(INSTALL_ROOT)/etc/ath/p2p/p2pdev.conf
	rm -rf $(INSTALL_ROOT)/etc/ath/wpa-sta.conf
	rm -rf $(INSTALL_ROOT)/home/atheros/Atheros-P2P
	rm -rf $(INSTALL_ROOT)/usr/bin/start-*
	rm -rf $(INSTALL_ROOT)/etc/udhcpc-p2p.*
	rm -rf $(INSTALL_ROOT)/etc/udhcpd-p2p.*
