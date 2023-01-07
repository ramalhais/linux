#!/bin/sh
for ALIAS in $(busybox --list); do
	if [ "$ALIAS" != "busybox" ] && [ "$ALIAS" != "sh" ]; then
		ln -s busybox "/bin/$ALIAS"
	fi
done
mount -t devtmpfs none /dev
mount -t proc none /proc
mount -t sysfs none /sys
#cat /etc/next-ascii.ans
mount LABEL=root /mnt
switch_root /mnt /init.sh
