#!/bin/sh

wan_ipaddr=192.168.0.19
gateway=10.10.10.254
server_ip=10.10.10.99
server_port=8081

iptables -t nat -I PREROUTING -d $wan_ipaddr -p tcp -m tcp --dport $server_port -j DNAT --to-destination $server_ip:$server_port
iptables -t nat -A POSTROUTING -s 10.10.10.0/255.255.255.0 -d $server_ip -p tcp -m tcp --dport $server_port -j SNAT --to-source $gateway 
iptables -A INPUT -d $server_ip -p tcp -m tcp --dport $server_port -i eth0 -j ACCEPT 