#!/bin/sh

IFNAME=$1
CMD=$2

echo $1
echo $2
echo "=================="

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



if [ "$CMD" = "CONNECTED" ]; then
	udhcpc -i $IFNAME -s /etc/udhcpc-p2p.script
fi

if [ "$CMD" = "DISCONNECTED" ]; then
	kill_daemon udhcpc
	ifconfig $IFNAME 0.0.0.0
fi
