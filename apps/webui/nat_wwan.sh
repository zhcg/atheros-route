#nat_wwan.sh
# iptables config

iptables -A FORWARD -j ACCEPT -i br0 -o $1  -m state --state NEW

iptables -A POSTROUTING -t nat -o $1 -j MASQUERADE


# WAN - LAN/WLAN access
iptables -I PREROUTING -t nat -i $1 -p tcp --dport 1000:20000 -j DNAT --to 10.10.10.254 -m state --state NEW
iptables -I FORWARD -i $1 -p tcp --dport 1000:20000 -d 10.10.10.254 -j ACCEPT

