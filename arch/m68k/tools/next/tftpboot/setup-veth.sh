set -x

# Configuration
HOST_IF=$(ip r|grep default|head -1|cut -d' ' -f5)
HOST_VIF=veth0
HOST_VIF_IP=192.168.111.1
GUEST_VIF=veth0guest

echo HOST_IF=$HOST_IF
echo HOST_VIF=$HOST_VIF
echo HOST_VIF_IP=$HOST_VIF_IP
echo GUEST_VIF=$GUEST_VIF

# Configure networking
ip link add $HOST_VIF type veth peer $GUEST_VIF
ethtool --offload $HOST_VIF tx off
ip addr add $HOST_VIF_IP/24 dev $HOST_VIF
ip link set $HOST_VIF up
ip link set $GUEST_VIF up

# NeXT PROM doesn't do ARP. Match these to dhcpd.conf.
arp -s 192.168.111.12 00:00:0f:00:30:90
arp -s 192.168.111.13 00:00:0f:00:f3:02
arp -s 192.168.111.14 00:00:0f:12:34:56
arp -s 192.168.111.15 00:00:0f:12:34:57

# Restart TFTPd and DHCPd
systemctl restart tftpd-hpa
#systemctl restart isc-dhcp-server
systemctl restart dhcpd

# Forwarding/SNAT
echo 1 > /proc/sys/net/ipv4/ip_forward
iptables -t nat -I POSTROUTING -o $HOST_IF -j MASQUERADE

