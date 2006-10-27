#!/bin/sh

#------------------------------------------------------------------------------
#
#  MSR Init Script
#
#  $Id$
#
#------------------------------------------------------------------------------

### BEGIN INIT INFO
# Provides:          msr
# Required-Start:    $local_fs $syslog $network
# Should-Start:      $time ntp ethercat
# Required-Stop:     $local_fs $syslog $network
# Should-Stop:       $time ntp ethercat
# Default-Start:     3 5
# Default-Stop:      0 1 2 6
# Short-Description: MSR module
# Description:
### END INIT INFO

#------------------------------------------------------------------------------

# <Customizing>

NAME="MSR EtherCAT sample"
BASE=/opt/etherlab
MSR_SERVER=$BASE/bin/msrserv.pl
MODULE=ec_msr_sample
RTAI_PATH=/usr/realtime
RTAI_MODULES="hal up" # sem math
DEVICE=msr
DEVMASK=664
GROUP=users

# </Customizing>

#------------------------------------------------------------------------------

. /etc/rc.status
rc_reset

#------------------------------------------------------------------------------

case "$1" in
    start)
	echo -n Starting $NAME" "

	# Insert RTAI modules
	for mod in $RTAI_MODULES; do
	    if ! lsmod | grep -q "^rtai_$mod"; then
		if ! insmod $RTAI_PATH/modules/rtai_$mod.ko; then
		    /bin/false
		    rc_status -v
		    rc_exit
		fi
	    fi
	done

	# Insert realtime module
	if ! modprobe $MODULE; then
	    /bin/false
	    rc_status -v
	    rc_exit
	fi

	#sleep 2

	# Create MSR device
	MAJOR=`cat /proc/devices | awk "\\$2==\"$DEVICE\" {print \\$1}"`
	rm -f /dev/${DEVICE}
	mknod /dev/${DEVICE} c $MAJOR 0
	chgrp $GROUP /dev/${DEVICE}
	chmod $DEVMASK /dev/${DEVICE}

	#sleep 1

	# Start MSR-Server
	startproc $MSR_SERVER 1>/dev/null 2>/dev/null
	rc_status -v
	;;

    stop)
	echo -n Shutting down $NAME" "

	if ! killproc $MSR_SERVER; then
	    /bin/false
	    rc_status -v
	    rc_exit
	fi

	if ! /sbin/rmmod $MODULE; then
	    /bin/false
	    rc_status -v
	    rc_exit
	fi

	# Remove stale nodes
	rm -f /dev/${DEVICE} /dev/${DEVICE}0

	rc_status -v
	;;

    restart)
	$0 stop || exit 1
	sleep 1
	$0 start

	rc_status
	;;

    status)
	echo -n "Checking for MSR module: "
	/sbin/lsmod | grep -q "^$MODULE"
	rc_status -v

	echo -n "Checking for MSR server: "
	checkproc $MSR_SERVER
	rc_status -v
	;;

    *)
	echo "Usage: $0 {start|stop|status|restart}"
	exit 1
	;;
esac

rc_exit