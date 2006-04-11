#!/bin/sh

#------------------------------------------------------------------------------
#
#  Realtime module install script
#
#  $Id: install.sh 5 2006-04-07 13:49:10Z fp $
#
#------------------------------------------------------------------------------

# Fetch parameters

if [ $# -ne 2 ]; then
    echo "Usage: $0 <MODULENAME> <KERNEL>"
    exit 1
fi

MODULENAME=$1
KERNEL=$2

MODULESDIR=/lib/modules/$KERNEL/kernel/drivers/rt

echo "Realtime installer"
echo "  target: $MODULENAME"
echo "  kernel: $KERNEL"

# Create target directory

if [ ! -d $MODULESDIR ]; then
    echo "  creating $MODULESDIR..."
    mkdir $MODULESDIR || exit 1
fi

# Install files

echo "  installing $MODULENAME..."
if ! cp $MODULENAME.ko $MODULESDIR/$MODULENAME.ko; then exit 1; fi

# Calculate dependencies

echo "  building module dependencies..."
depmod

# Finish

echo "done."
exit 0

#------------------------------------------------------------------------------
