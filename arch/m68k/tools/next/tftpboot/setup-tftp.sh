#!/bin/bash
ip tuntap add mode tap tap0
ip addr add dev tap0 192.168.111.1/24
ip link set tap0 up
systemctl restart isc-dhcp-server
arp -s 192.168.111.10 00:00:0f:00:f3:02
tcpdump -ln -i tap0 -vv -s0 -X
#cd ~ramalhais/git/qemu/bin/debug/native
#make && ./qemu-system-m68k -machine next-cube -nic tap,script=no,downscript=no,ifname=tap0 -serial stdio
# -serial telnet:localhost:4321,server,nowait
# http://n-1.nl/next/hardware/info/rom-monitor.html#SystemTestDiag
