#!/bin/sh

#------------------------------------------------------------------------------
#
#  EtherCAT install script
#
#  $Id$
#
#------------------------------------------------------------------------------

CONFIGFILE=/etc/sysconfig/ethercat

#------------------------------------------------------------------------------

# install function

install()
{
    echo "    $1"
    if ! cp $1 $INSTALLDIR; then exit 1; fi
}

#------------------------------------------------------------------------------

# Fetch parameter

if [ $# -ne 2 ]; then
    echo "Usage: $0 <KERNEL> <DEVICEINDEX>"
    exit 1
fi

KERNEL=$1
DEVICEINDEX=$2

INSTALLDIR=/lib/modules/$KERNEL/kernel/drivers/net
echo "EtherCAT installer - Kernel: $KERNEL"

# Copy files

echo "  installing modules..."
install master/ec_master.ko
install devices/ec_8139too.ko

# Update dependencies

echo "  building module dependencies..."
depmod

# Create configuration file

if [ -f $CONFIGFILE ]; then
    echo "  notice: using existing configuration file."
else
    echo "  creating $CONFIGFILE..."
    echo "DEVICEINDEX=$DEVICEINDEX" > $CONFIGFILE || exit 1
fi

# Install rc script

echo "  installing startup script..."
cp ethercat.sh /etc/init.d/ethercat || exit 1
if [ ! -L /usr/sbin/rcethercat ]; then
    ln -s /etc/init.d/ethercat /usr/sbin/rcethercat || exit 1
fi

# Finish

echo "EtherCAT installer done."
exit 0

#------------------------------------------------------------------------------
