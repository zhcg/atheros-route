#!/bin/sh
#
#    WFA-start.sh : Start script for Wi-Fi Direct Testing.
#

#
#    Parameters
#
KVER=`uname -r | cut -f 1 -d '-'`
MODULE_PATH=/lib/modules/$KVER/net

BIN_PATH=/sbin
WFD_PATH=/usr/bin

P2P_CONF_FILE=/etc/ath/p2p/p2pdev.conf
P2P_ACT_FILE=/home/atheros/Atheros-P2P/scripts/p2p-action-pb44.sh

WLAN_CONF_FILE=/etc/ath/wpa-sta.conf
WLAN_ACT_FILE=/home/atheros/Atheros-P2P/scripts/wlan-action.sh

#
#    Install Driver
#
echo "=============Install Driver..."
${BIN_PATH}/insmod ${MODULE_PATH}/adf.ko
${BIN_PATH}/insmod ${MODULE_PATH}/asf.ko
${BIN_PATH}/insmod ${MODULE_PATH}/ath_hif_usb.ko
${BIN_PATH}/insmod ${MODULE_PATH}/ath_htc_hst.ko
${BIN_PATH}/insmod ${MODULE_PATH}/ath_hal.ko
${BIN_PATH}/insmod ${MODULE_PATH}/ath_dfs.ko
${BIN_PATH}/insmod ${MODULE_PATH}/ath_rate_atheros.ko
${BIN_PATH}/insmod ${MODULE_PATH}/ath_dev.ko
${BIN_PATH}/insmod ${MODULE_PATH}/umac.ko
${BIN_PATH}/insmod ${MODULE_PATH}/ath_usbdrv.ko
sleep 2

#
#    Create VAPs
#
echo "=============Create VAPs..."
${BIN_PATH}/wlanconfig wlan create wlandev wifi0 wlanmode sta
${BIN_PATH}/wlanconfig wlan create wlandev wifi0 wlanmode p2pdev
sleep 1

#
#    Start wpa_supplicant
#
echo "=============Start wpa_supplicant..."
${BIN_PATH}/wpa_supplicant -Dathr -iwlan1 -c ${P2P_CONF_FILE} -N -i wlan0 -Dathr -c ${WLAN_CONF_FILE} &
sleep 2

#
#    Other actions before testing
#
echo "=============Action before testing..."
${BIN_PATH}/iwpriv wlan0 shortgi 0
echo 300 > /proc/sys/net/ipv4/neigh/default/base_reachable_time
echo 0 > /proc/sys/net/ipv4/neigh/default/ucast_solicit

#
#    Start WFD automation & wpa_cli action control
#
echo "=============Start wpa_cli action control..."
${BIN_PATH}/wpa_cli -i wlan0 -a ${WLAN_ACT_FILE} &
sleep 1
${BIN_PATH}/wpa_cli -i wlan1 -a ${P2P_ACT_FILE} &
sleep 1
echo "=============Start WFD automation..."
wfa_dut lo 8000 &
export WFA_ENV_AGENT_IPADDR=127.0.0.1 
export WFA_ENV_AGENT_PORT=8000 
wfa_ca lo 9000 &
sleep 1

echo "=============Done!"
