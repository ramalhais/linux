TARGET_ADDR=192.168.111.1
INITIATOR=$(hostname -f)

#
# Make a specific target disk available
#
#TARGET_DISK=nextdisk
#TARGET_GROUP=1
# ex: iscsistart -i NeXT -t nextdisk -g 1 -a 192.168.111.1
#iscsistart --initiatorname=$INITIATOR --targetname=$TARGET_DISK --tgpt=$TARGET_GROUP --address=$TARGET_ADDR

#
# Make all target disks from $TARGET_ADDR available
#
# File needed by iscsid
INITIATOR_FILE=/etc/iscsi/initiatorname.iscsi
echo "InitiatorName=$INITIATOR" > $INITIATOR_FILE
# iscsiadm needs to communicate with iscsid. using ofreground because otherwise the process disappears.
iscsid --foreground&
# List targets
iscsiadm -m discovery -t sendtargets -p $TARGET_ADDR -l
# Login to all discovered targets. Same as -l in discovery mode above
#iscsiadm -m node -L all
