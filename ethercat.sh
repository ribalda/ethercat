#!/bin/sh

#------------------------------------------------------------------------------
#
#  EtherCAT rc script
#
#  $Id$
#
#------------------------------------------------------------------------------

CONFIGFILE=/etc/sysconfig/ethercat

#------------------------------------------------------------------------------

print_usage()
{
    echo "Usage: $0 { start | stop | restart }"
}

unload_module()
{
    if lsmod | grep ^$1 > /dev/null; then
	echo "  unloading module \"$1\"..."
	rmmod $1 || exit 1
    fi
}

#------------------------------------------------------------------------------

# Get parameters
if [ $# -eq 0 ]; then
    print_usage
    exit 1
fi

ACTION=$1

# Load configuration from sysconfig

if [ -f $CONFIGFILE ]; then
    . $CONFIGFILE
else
    echo "ERROR: Configuration file \"$CONFIGFILE\" not found!"
    exit 1
fi

case $ACTION in
    start | restart)
	echo "Starting EtherCAT master..."

	# remove modules
	unload_module 8139too
	unload_module 8139cp
	unload_module ec_8139too
	unload_module ec_master

	echo "  loading master modules..."
	if ! modprobe ec_8139too ec_device_index=$DEVICEINDEX; then
	    echo "ERROR: Failed to load module!"
	    exit 1
	fi
	;;

    stop)
	echo "Stopping EtherCAT master..."
	unload_module ec_8139too
	unload_module ec_master
	if ! modprobe 8139too; then
	    echo "Warning: Failed to restore 8139too module."
	fi
	;;

    *)
	print_usage
	exit 1
esac

echo "done."
exit 0

#------------------------------------------------------------------------------
