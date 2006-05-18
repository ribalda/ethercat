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
# Provides:          IgH EtherCAT master
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

. /etc/rc.status
rc_reset

case "$1" in
    start)
	echo -n "Starting EtherCAT master... "

	for mod in 8139too 8139cp; do
		if lsmod | grep "^$mod " > /dev/null; then
			if ! rmmod $mod; then
				/bin/false
				rc_status -v
				rc_exit
			fi;
		fi;
	done

	modprobe ec_8139too ec_device_index=$DEVICEINDEX

	rc_status -v
	;;

    stop)
	echo -n "Shutting down EtherCAT master... "

	for mod in ec_8139too ec_master; do
		if lsmod | grep "^$mod " > /dev/null; then
			if ! rmmod $mod; then
				/bin/false
				rc_status -v
				rc_exit
			fi;
		fi;
	done

	if ! modprobe 8139too; then
	    echo "Warning: Failed to restore 8139too module."
	fi

	rc_status -v
	;;

    restart)
	$0 stop
	$0 start

	rc_status
	;;

    status)
	echo -n "Checking for EtherCAT... "

	lsmod | grep "^ec_master " > /dev/null
	master_running=$?
	lsmod | grep "^ec_8139too " > /dev/null
	device_running=$?
	test $master_running -eq 0 -a $device_running -eq 0

	rc_status -v
	;;
esac
rc_exit

#------------------------------------------------------------------------------
