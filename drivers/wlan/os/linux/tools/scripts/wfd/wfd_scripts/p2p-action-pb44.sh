#!/bin/sh

IFNAME=$1
CMD=$2
echo $1
echo $2

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

if [ "$CMD" = "P2P-GROUP-STARTED" ]; then
    GIFNAME=$3
    echo $3
    if [ "$4" = "GO" ]; then
        kill_daemon udhcpd
        kill_daemon udhcpc
        ifconfig $GIFNAME 192.168.42.1 up
        touch /var/run/udhcpd-p2p.leases
        sleep 1
        udhcpd /etc/udhcpd-p2p.conf
    fi
    if [ "$4" = "client" ]; then
    	kill_daemon udhcpd
    	kill_daemon udhcpc
        udhcpc -i $GIFNAME -s /etc/udhcpc-p2p.script
    fi
fi                          

if [ "$CMD" = "P2P-GROUP-REMOVED" ]; then
    GIFNAME=$3
    if [ "$4" = "GO" ]; then
        kill_daemon udhcpd
        ifconfig $GIFNAME 0.0.0.0
    fi
    if [ "$4" = "client" ]; then
        kill_daemon udhcpc 
        ifconfig $GIFNAME 0.0.0.0
    fi
fi

if [ "$CMD" = "P2P-CROSS-CONNECT-ENABLE" ]; then
    GIFNAME=$3
    UPLINK=$4
    # enable NAT/masquarade $GIFNAME -> $UPLINK
    iptables -P FORWARD DROP
    iptables -t nat -A POSTROUTING -o $UPLINK -j MASQUERADE
    iptables -A FORWARD -i $UPLINK -o $GIFNAME -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -i $GIFNAME -o $UPLINK -j ACCEPT
    echo 1 > /proc/sys/net/ipv4/ip_forward
fi

if [ "$CMD" = "P2P-CROSS-CONNECT-DISABLE" ]; then
    GIFNAME=$3
    UPLINK=$4
    # disable NAT/masquarade $GIFNAME -> $UPLINK
    echo 0 > /proc/sys/net/ipv4/ip_forward
    iptables -t nat -D POSTROUTING -o $UPLINK -j MASQUERADE
    iptables -D FORWARD -i $UPLINK -o $GIFNAME -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -D FORWARD -i $GIFNAME -o $UPLINK -j ACCEPT
fi
