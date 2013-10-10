#!/bin/sh
#
#    WFA-stop.sh : Stop script for Wi-Fi Direct Testing.
#

#
#    Parameters
#

BIN_PATH=/sbin

kill_daemon() {
    NAME=$1
    PLIST=`ps | grep $NAME | cut -f2 -d" "`
    if [ ${#PLIST} -eq 0 ]; then
	PLIST=`ps | grep $NAME | cut -f3 -d" "`
    fi
    PID=`echo $PLIST | cut -f1 -d" "`
    if [ $PID -gt 0 ]; then
        if ps $PID | grep -q $NAME; then
            kill $PID
        fi
    fi
}

#
#    Stop WFD automation & wpa_cli action control
#
echo "=============Stop DHCP..."
kill_daemon udhcpd
kill_daemon udhcpc
sleep 1
echo "=============Stop wpa_cli action control..."
kill_daemon wpa_cli
sleep 1
echo "=============Stop WFD automation"
kill_daemon wfa_dut
kill_daemon wfa_ca
sleep 1

#
#    Stop wpa_supplicant
#
echo "=============Stop wpa_supplicant..."
${BIN_PATH}/wpa_cli terminate
sleep 2

#
#    Destroy VAPs
#
echo "=============Destroy VAPs..."
${BIN_PATH}/ifconfig wlan2 down
${BIN_PATH}/ifconfig wlan1 down
${BIN_PATH}/ifconfig wlan0 down
${BIN_PATH}/wlanconfig wlan2 destroy
${BIN_PATH}/wlanconfig wlan1 destroy
${BIN_PATH}/wlanconfig wlan0 destroy
sleep 2

#
#    Uninstall Driver
#
echo "=============Uninstall Driver..."
${BIN_PATH}/rmmod ath_usbdrv
${BIN_PATH}/rmmod umac
${BIN_PATH}/rmmod ath_dev
${BIN_PATH}/rmmod ath_rate_atheros
${BIN_PATH}/rmmod ath_dfs
${BIN_PATH}/rmmod ath_hal
${BIN_PATH}/rmmod ath_htc_hst
${BIN_PATH}/rmmod ath_hif_usb
${BIN_PATH}/rmmod asf
${BIN_PATH}/rmmod adf
sleep 1

echo "=============Done!"
