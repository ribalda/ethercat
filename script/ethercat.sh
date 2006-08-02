#!/bin/sh

#------------------------------------------------------------------------------
#
#  Init script for EtherCAT
#
#  $Id$
#
#  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
#
#  This file is part of the IgH EtherCAT Master.
#
#  The IgH EtherCAT Master is free software; you can redistribute it
#  and/or modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  The IgH EtherCAT Master is distributed in the hope that it will be
#  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with the IgH EtherCAT Master; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#  The right to use EtherCAT Technology is granted and comes free of
#  charge under condition of compatibility of product made by
#  Licensee. People intending to distribute/sell products based on the
#  code, have to sign an agreement to guarantee that products using
#  software based on IgH EtherCAT master stay compatible with the actual
#  EtherCAT specification (which are released themselves as an open
#  standard) as the (only) precondition to have the right to use EtherCAT
#  Technology, IP and trade marks.
#
#------------------------------------------------------------------------------

### BEGIN INIT INFO
# Provides:          ethercat
# Required-Start:    $local_fs $syslog $network
# Should-Start:      $time
# Required-Stop:     $local_fs $syslog $network
# Should-Stop:       $time
# Default-Start:     3 5
# Default-Stop:      0 1 2 6
# Short-Description: IgH EtherCAT master modules
# Description:
### END INIT INFO

#------------------------------------------------------------------------------

ETHERCAT_CONFIG=/etc/sysconfig/ethercat

test -r $ETHERCAT_CONFIG || { echo "$ETHERCAT_CONFIG not existing";
	if [ "$1" = "stop" ]; then exit 0;
	else exit 6; fi; }

. $ETHERCAT_CONFIG

#------------------------------------------------------------------------------

#
#  Function for setting up the EoE bridge
#
build_eoe_bridge() {
        if [ -z "$EOE_BRIDGE" ]; then return; fi

	EOEIF=`/sbin/ifconfig -a | grep -o -E "^eoe[0-9]+ "`

	# add bridge, if it does not already exist
	if ! /sbin/brctl show | grep -E -q "^$EOE_BRIDGE"; then
	        if ! /sbin/brctl addbr $EOE_BRIDGE; then
		        /bin/false
			rc_status -v
			rc_exit
		fi
	fi

    # check if specified interfaces are bridged
	for interf in $EOEIF $EOE_EXTRA_INTERFACES; do
	        # interface is already part of the bridge 
	        if /sbin/brctl show $EOE_BRIDGE | grep -E -q $interf
		        then continue
		fi
		# clear IP address and open interface
		if ! /sbin/ifconfig $interf 0.0.0.0 up; then
		        /bin/false
			rc_status -v
			rc_exit
		fi
		# add interface to the bridge
		if ! /sbin/brctl addif $EOE_BRIDGE $interf; then
		        /bin/false
			rc_status -v
			rc_exit
		fi
	done

	# configure IP on bridge
	if [ -n "$EOE_IP_ADDRESS" -a -n "$EOE_IP_NETMASK" ]; then
	        if ! /sbin/ifconfig $EOE_BRIDGE $EOE_IP_ADDRESS \
		        netmask $EOE_IP_NETMASK; then
		        /bin/false
			rc_status -v
			rc_exit
		fi
	fi

	# open bridge
	if ! /sbin/ifconfig $EOE_BRIDGE up; then
	        /bin/false
		rc_status -v
		rc_exit
	fi

	# install new default gateway
	if [ -n "$EOE_GATEWAY" ]; then
	        while /sbin/route -n | grep -E -q "^0.0.0.0"; do
		        if ! /sbin/route del default; then
			        echo "Failed to remove route!" 1>&2
				/bin/false
				rc_status -v
				rc_exit
			fi
		done
		if ! /sbin/route add default gw $EOE_GATEWAY; then
		        /bin/false
			rc_status -v
			rc_exit
		fi
	fi
}

#------------------------------------------------------------------------------

. /etc/rc.status
rc_reset

case "$1" in
    start)
	echo -n "Starting EtherCAT master "

	if [ -z "$DEVICE_INDEX" ]; then
	    echo "ERROR: DEVICE_INDEX not set!"
	    /bin/false
	    rc_status -v
	    rc_exit
	fi

	if [ -z "$EOE_INTERFACES" ]; then
		if [ -n "$EOE_DEVICES" ]; then # support legacy sysconfig files
			EOE_INTERFACES=$EOE_DEVICES
		else
			EOE_INTERFACES=0
		fi
	fi

	# unload conflicting modules at first
	for mod in 8139too 8139cp; do
		if lsmod | grep "^$mod " > /dev/null; then
			if ! rmmod $mod; then
				/bin/false
				rc_status -v
				rc_exit
			fi
		fi
	done

	# load master module
	if ! modprobe ec_master ec_eoeif_count=$EOE_INTERFACES; then
	    /bin/false
	    rc_status -v
	    rc_exit
	fi

	# load device module
	if ! modprobe ec_8139too ec_device_index=$DEVICE_INDEX; then
	    /bin/false
	    rc_status -v
	    rc_exit
	fi

	# build EoE bridge
	build_eoe_bridge

	rc_status -v
	;;

    stop)
	echo -n "Shutting down EtherCAT master "

	# unload modules
	for mod in ec_8139too ec_master; do
		if lsmod | grep "^$mod " > /dev/null; then
			if ! rmmod $mod; then
				/bin/false
				rc_status -v
				rc_exit
			fi;
		fi;
	done

	sleep 1

	# reload previous modules
	if ! modprobe 8139too; then
	    echo "Warning: Failed to restore 8139too module."
	fi

	rc_status -v
	;;

    restart)
	$0 stop || exit 1

	sleep 1

	$0 start

	rc_status
	;;

    status)
	echo -n "Checking for EtherCAT "

	lsmod | grep "^ec_master " > /dev/null
	master_running=$?
	lsmod | grep "^ec_8139too " > /dev/null
	device_running=$?

	# master module and device module loaded?
	test $master_running -eq 0 -a $device_running -eq 0

	rc_status -v
	;;

    bridge)
	echo -n "Building EoE bridge "

	build_eoe_bridge

	rc_status -v
	;;

    *)
	echo "USAGE: $0 {start|stop|restart|status|bridge}"
	;;
esac
rc_exit

#------------------------------------------------------------------------------
