#!/bin/sh

#------------------------------------------------------------------------------
#
#  Realtime module install script
#
#  $Id: install.sh 5 2006-04-07 13:49:10Z fp $
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
