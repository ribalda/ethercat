#!/bin/sh

#------------------------------------------------------------------------------
#
#  EtherCAT rc script
#
#  $Id$
#
#  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
#
#  This file is part of the IgH EtherCAT Master.
#
#  The IgH EtherCAT Master is free software; you can redistribute it
#  and/or modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; version 2 of the License.
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
#------------------------------------------------------------------------------

### BEGIN INIT INFO
# Provides:          EtherCAT
# Required-Start:
# Should-Start:
# Required-Stop:
# Should-Stop:
# Default-Start:     3 5
# Default-Stop:      0 1 2 6
# Short-Description: EtherCAT master driver and network device
# Description:
### END INIT INFO

#------------------------------------------------------------------------------

ETHERCAT_CONFIG=/etc/sysconfig/ethercat

test -r $ETHERCAT_CONFIG || { echo "$ETHERCAT_CONFIG not existing";
	if [ "$1" = "stop" ]; then exit 0;
	else exit 6; fi; }

. $ETHERCAT_CONFIG

#------------------------------------------------------------------------------

# Shell functions sourced from /etc/rc.status:
#      rc_check         check and set local and overall rc status
#      rc_status        check and set local and overall rc status
#      rc_status -v     be verbose in local rc status and clear it afterwards
#      rc_status -v -r  ditto and clear both the local and overall rc status
#      rc_status -s     display "skipped" and exit with status 3
#      rc_status -u     display "unused" and exit with status 3
#      rc_failed        set local and overall rc status to failed
#      rc_failed <num>  set local and overall rc status to <num>
#      rc_reset         clear both the local and overall rc status
#      rc_exit          exit appropriate to overall rc status
#      rc_active        checks whether a service is activated by symlinks
. /etc/rc.status

# Reset status of this service
rc_reset

# Return values acc. to LSB for all commands but status:
# 0	  - success
# 1       - generic or unspecified error
# 2       - invalid or excess argument(s)
# 3       - unimplemented feature (e.g. "reload")
# 4       - user had insufficient privileges
# 5       - program is not installed
# 6       - program is not configured
# 7       - program is not running
# 8--199  - reserved (8--99 LSB, 100--149 distrib, 150--199 appl)
#
# Note that starting an already running service, stopping
# or restarting a not-running service as well as the restart
# with force-reload (in case signaling is not supported) are
# considered a success.

case "$1" in
    start)
	echo -n "Starting EtherCAT master... "

	# remove incompatible modules
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

	# Return value is slightly different for the status command:
	# 0 - service up and running
	# 1 - service dead, but /var/run/  pid  file exists
	# 2 - service dead, but /var/lock/ lock file exists
	# 3 - service not running (unused)
	# 4 - service status unknown :-(
	# 5--199 reserved (5--99 LSB, 100--149 distro, 150--199 appl.)

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
