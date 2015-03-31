iptables -F
iptables -P INPUT ACCEPT
iptables -P FORWARD ACCEPT
iptables -P OUTPUT ACCEPT
modprobe tun
./shinobi -h tokai-rbc.co.cc -d4


shinobi_hub
shinobi
ifconfig shino0 192.168.1.220 up
#route add 192.168.1.100 gw 192.168.1.220
echo 1 > /proc/sys/net/ipv4/ip_forward
iptables -t nat -F
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
#./L2Forwarder shino0 eth0
#プロミスキャスモードへ
(mount error(115): Operation now in progress)
iptables -t nat -A POSTROUTING -o eth0 -j SNAT --to-source 192.168.0.220 (Unable to find suitable address.)


ifconfig shino0 192.168.1.100 up
route add 192.168.0.250 gw 192.168.1.100
ping 192.168.1.220
ping 192.168.0.250 (ARP不可)

mount -t cifs //192.168.0.250/ /mnt/cifs/ -o user=tokairbc


traceroute 192.168.0.250
iptables -t nat -A POSTROUTING -o shino0 -j MASQUERADE


iptables -F
iptables -P INPUT ACCEPT
iptables -P FORWARD ACCEPT
iptables -P OUTPUT ACCEPT
./shinobi_hub -d 4
./shinobi -d 4
ifconfig shino0 192.168.1.1 up



brctl addbr br0
brctl addif br0 eth0
brctl addif br0 shino0


route add 192.168.0.50 gw 192.168.0.51

route add 192.168.0.51 gw 192.168.0.50
route add 192.168.0.250 gw 192.168.0.50


route del -net 192.168.0.0 netmask 255.255.255.0 wlan0
route add -net 192.168.0.0 netmask 255.255.255.0 shino0

iptables -I FORWARD -i shino+ -d 192.168.0.0/24 -j ACCEPT


iptables -t nat -L

iptables -t nat -A POSTROUTING -o shino0 -j SNAT --to-source 192.168.0.220
iptables -A FORWARD -s 192.168.1.0/24 -o eth0 -j ACCEPT

iptables -t nat -A POSTROUTING -o eth0 -s 192.168.1.0/24 -j MASQUERADE
