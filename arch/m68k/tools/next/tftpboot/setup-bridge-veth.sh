set -x

# Configuration
HOST_IF=$(ip r|grep default|head -1|cut -d' ' -f5)
SECOND_IF=ens37
BRIDGE_IF=br0
BRIDGE_IP=192.168.111.1
HOST_VIF=veth0
GUEST_VIF=veth0guest

echo HOST_IF=$HOST_IF
echo BRIDGE_IF=$BRIDGE_IF
echo BRIDGE_IP=$BRIDGE_IP
echo HOST_VIF=$HOST_VIF
echo GUEST_VIF=$GUEST_VIF

# Configure virtual interfaces for guest and host
ip link add $HOST_VIF type veth peer $GUEST_VIF
ethtool --offload $HOST_VIF tx off
ip link set $HOST_VIF up
ip link set $GUEST_VIF up

# Configure bridge
ip link add $BRIDGE_IF type bridge
ip addr add $BRIDGE_IP/24 dev $BRIDGE_IF
ip link set $BRIDGE_IF up

# Add second nic and host virtual interface to bridge
ip link set $SECOND_IF master $BRIDGE_IF
ip link set $HOST_VIF master $BRIDGE_IF

# NeXT PROM doesn't do ARP. Match these to dhcpd.conf.
arp -s 192.168.111.12 00:00:0f:00:30:90
arp -s 192.168.111.13 00:00:0f:00:f3:02
arp -s 192.168.111.14 00:00:0f:12:34:56
arp -s 192.168.111.15 00:00:0f:00:b5:fc
arp -s 192.168.111.16 00:00:0f:00:cd:81

# Restart TFTPd and DHCPd
systemctl restart tftpd-hpa
systemctl restart isc-dhcp-server
#systemctl restart dhcpd

# Forwarding/SNAT
echo 1 > /proc/sys/net/ipv4/ip_forward
iptables -t nat -I POSTROUTING -o $HOST_IF -j MASQUERADE
