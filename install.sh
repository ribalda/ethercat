#!/bin/sh

#------------------------------------------------------------------------------
#
#  EtherCAT install script
#
#  $Id$
#
#------------------------------------------------------------------------------

# install function

install()
{
    echo "  installing $1"
    if ! cp $1 $INSTALLDIR; then exit 1; fi
}

#------------------------------------------------------------------------------

# Fetch parameter

if [ $# -eq 0 ]; then
    echo "Usage: $0 <INSTALLDIR>"
    exit 1
fi

INSTALLDIR=$1
echo "EtherCAT installer. Target: $INSTALLDIR"

# Create installation directory

if [ ! -d $INSTALLDIR ]; then
    echo "  creating target directory."
    if ! mkdir $INSTALLDIR; then exit 1; fi
fi

# Copy files

install master/ec_master.ko
install devices/ec_8139too.ko

# Finished

exit 0

#------------------------------------------------------------------------------
