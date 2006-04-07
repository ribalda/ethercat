#!/bin/sh

#------------------------------------------------------------------------------
#
#  EtherCAT install script
#
#  $Id$
#
#------------------------------------------------------------------------------

# Fetch parameter

if [ $# -eq 0 ]; then
    echo "Usage: $0 <INSTALLDIR>"
    exit 1
fi

INSTALLDIR=$1

# Create install directory

if [ ! -d $INSTALLDIR ]; then
    echo "Creating directory $INSTALLDIR..."
    if ! mkdir $INSTALLDIR; then exit 1; fi
fi

# Copy files

if ! cp master/ec_master.ko   $INSTALLDIR; then exit -1; fi
if ! cp devices/ec_8139too.ko $INSTALLDIR; then exit -1; fi

# Finished

exit 0

#------------------------------------------------------------------------------
