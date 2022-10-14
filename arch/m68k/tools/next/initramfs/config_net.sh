# Setup loopback
ip address add  127.0.0.2/8 dev lo
ip link set lo up

# Setup networking
#ip address add  192.168.10.2/24 dev eth0
#ip route add default via 192.168.1.1
#ip link set eth0 up

udhcpc

#mount -t nfs -o vers=2,nolock 10.0.2.254:/private/tftpboot /mnt
