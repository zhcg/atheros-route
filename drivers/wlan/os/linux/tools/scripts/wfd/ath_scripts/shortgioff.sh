
iwpriv wlan0 shortgi 0
echo 60 > /proc/sys/net/ipv4/neigh/default/base_reachable_time
echo 0 > /proc/sys/net/ipv4/neigh/default/ucast_solicit
iwpriv wlan1 get_shortgi
